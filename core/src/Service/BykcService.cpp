#include <UBAANext/Service/BykcService.hpp>

#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/BykcParser.hpp>
#include <UBAANext/Protocol/AuthorizedDownstreamRequestExecutor.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

#include <charconv>
#include <chrono>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

namespace UBAANext {

namespace {

constexpr const char *rsa_public_key_base64 = "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDlHMQ3B5GsWnCe7Nlo1YiG/YmHdlOiKOST5aRm4iaqYSvhvWmwcigoyWTM+8bv2+sf6nQBRDWTY4KmNV7DBk1eDnTIQo6ENA31k5/tYCLEXgjPbEjCK9spiyB62fCT6cqOhbamJB0lcDJRO6Vo1m3dy+fD0jbxfDVBBNtyltIsDQIDAQAB";
constexpr const char *key_chars = "ABCDEFGHJKMNPQRSTWXYZabcdefhijkmnprstwxyz2345678";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    auto value = Protocol::header_value(response, name);
    auto newline = value.find('\n');
    return newline == std::string::npos ? value : value.substr(0, newline);
}

std::string resolve_redirect_url(const std::string &base_url, const std::string &location) {
    return Protocol::resolve_location(base_url, location);
}

std::string extract_bykc_token(const std::string &url) {
    return Protocol::extract_query_parameter_anywhere(url, "token");
}

Result<long long> parse_positive_id(const std::string &value, const std::string &name) {
    long long parsed = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed <= 0) {
        return make_error(ErrorCode::InvalidArgument, name + " 必须是正整数");
    }
    return parsed;
}

void apply_bykc_login_headers(HttpRequest &request) {
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
}

bool response_is_login(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response, {}, false);
}

Result<std::string> acquire_bykc_token(IHttpClient &http_client, ConnectionMode mode, const std::string &url, const char *failure_message) {
    std::string current_url = url;
    for (int redirects = 0; redirects < 8; ++redirects) {
        auto current_token = extract_bykc_token(current_url);
        if (!current_token.empty()) return current_token;

        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, mode);
        apply_bykc_login_headers(request);
        Protocol::disable_transport_redirects(request);

        auto response = http_client.send(request);
        if (!response) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::Unavailable,
                                                         std::string(failure_message) + ": " + response.error().message,
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        if ((response->status_code < 300 || response->status_code >= 400) && response_is_login(*response)) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::SsoRequired,
                                                         "博雅会话已过期，请重新登录",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }

        auto location = header_value(*response, "Location");
        auto location_token = extract_bykc_token(location);
        if (!location_token.empty()) return location_token;
        auto body_token = extract_bykc_token(response->body);
        if (!body_token.empty()) return body_token;
        if (response->status_code < 300 || response->status_code >= 400) return std::string{};
        if (location.empty()) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::ProtocolError,
                                                         "博雅跳转缺少 Location",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        current_url = resolve_redirect_url(current_url, location);
    }
    auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                 Protocol::DownstreamActivationStage::RedirectFollow,
                                                 Protocol::DownstreamSessionState::Unavailable,
                                                 "博雅跳转次数过多",
                                                 Protocol::redact_url_query(current_url));
    return make_error(error.code, Protocol::to_error(error).message);
}

std::string bytes_to_hex(const std::vector<unsigned char> &bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : bytes) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

std::vector<unsigned char> string_bytes(const std::string &text) {
    return {text.begin(), text.end()};
}

std::string generate_aes_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, std::char_traits<char>::length(key_chars) - 1);
    std::string key;
    key.reserve(16);
    for (int i = 0; i < 16; ++i) key.push_back(key_chars[dist(gen)]);
    return key;
}

Result<std::tuple<std::string, std::string, std::string, std::string>> encrypt_request(const std::string &json, ICryptoProvider &crypto) {
    auto key = generate_aes_key();
    auto encrypted = crypto.aes_ecb_pkcs7_encrypt(string_bytes(json), key);
    if (!encrypted) return make_error(encrypted.error().code, encrypted.error().message);
    auto digest = crypto.sha1_digest(string_bytes(json));
    if (!digest) return make_error(digest.error().code, digest.error().message);
    auto ak = crypto.rsa_pkcs1_encrypt_base64(string_bytes(key), rsa_public_key_base64);
    if (!ak) return make_error(ak.error().code, ak.error().message);
    auto sign_hex = bytes_to_hex(*digest);
    auto sk = crypto.rsa_pkcs1_encrypt_base64(string_bytes(sign_hex), rsa_public_key_base64);
    if (!sk) return make_error(sk.error().code, sk.error().message);
    return std::make_tuple(base64_encode(*encrypted), *ak, *sk, key);
}

std::int64_t now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    if (json[key].is_boolean()) return json[key].get<bool>() ? "true" : "false";
    return {};
}

struct BykcSignPoint {
    double lat = 0.0;
    double lng = 0.0;
    double radius = 0.0;
};

struct BykcSignLocation {
    double lat = 0.0;
    double lng = 0.0;
};

bool json_double(const nlohmann::json &json, const char *key, double &value) {
    if (!json.contains(key) || json[key].is_null()) return false;
    try {
        if (json[key].is_number()) {
            value = json[key].get<double>();
            return std::isfinite(value);
        }
        if (json[key].is_string()) {
            const auto text = json[key].get<std::string>();
            std::size_t consumed = 0;
            value = std::stod(text, &consumed);
            return consumed == text.size() && std::isfinite(value);
        }
    } catch (...) {
        return false;
    }
    return false;
}

std::vector<BykcSignPoint> parse_sign_points(const std::string &sign_config) {
    if (sign_config.empty()) return {};
    auto config = nlohmann::json::parse(sign_config, nullptr, false);
    if (config.is_string()) config = nlohmann::json::parse(config.get<std::string>(), nullptr, false);
    if (config.is_discarded() || !config.is_object()) return {};
    const auto list = config.contains("signPointList") && config["signPointList"].is_array()
                          ? config["signPointList"]
                          : (config.contains("signPoints") && config["signPoints"].is_array() ? config["signPoints"] : nlohmann::json::array());
    std::vector<BykcSignPoint> points;
    for (const auto &point : list) {
        double lat = 0.0;
        double lng = 0.0;
        double radius = 0.0;
        if (!json_double(point, "lat", lat) || !json_double(point, "lng", lng) || !json_double(point, "radius", radius)) continue;
        if (radius <= 0.0 || lat < -90.0 || lat > 90.0 || lng < -180.0 || lng > 180.0) continue;
        points.push_back({lat, lng, radius});
    }
    return points;
}

double degrees_to_radians(double value) {
    constexpr double pi = 3.14159265358979323846;
    return value * pi / 180.0;
}

double radians_to_degrees(double value) {
    constexpr double pi = 3.14159265358979323846;
    return value * 180.0 / pi;
}

BykcSignLocation destination_point(const BykcSignPoint &point, double distance, double angle) {
    constexpr double earth_radius_meters = 6371000.0;
    const double angular_distance = distance / earth_radius_meters;
    const double lat = degrees_to_radians(point.lat);
    const double lng = degrees_to_radians(point.lng);
    const double destination_lat = std::asin(std::sin(lat) * std::cos(angular_distance) + std::cos(lat) * std::sin(angular_distance) * std::cos(angle));
    const double destination_lng = lng + std::atan2(std::sin(angle) * std::sin(angular_distance) * std::cos(lat), std::cos(angular_distance) - std::sin(lat) * std::sin(destination_lat));
    return {radians_to_degrees(destination_lat), radians_to_degrees(destination_lng)};
}

Result<BykcSignLocation> random_sign_location_from_config(const std::string &sign_config) {
    auto points = parse_sign_points(sign_config);
    if (points.empty()) return make_error(ErrorCode::InvalidArgument, "博雅课程未返回签到范围，无法自动生成签到位置");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::size_t> point_dist(0, points.size() - 1);
    std::uniform_real_distribution<double> unit_dist(0.0, 1.0);
    const auto &point = points[point_dist(gen)];
    const auto distance = point.radius * std::sqrt(unit_dist(gen));
    constexpr double two_pi = 6.28318530717958647692;
    return destination_point(point, distance, unit_dist(gen) * two_pi);
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord profile_to_record(const Model::BykcProfile &profile) {
    return make_record(profile.id, profile.real_name, "active", {
        {"employeeId", profile.employee_id},
        {"studentNo", profile.student_no},
        {"studentType", profile.student_type},
        {"classCode", profile.class_code},
        {"collegeName", profile.college_name},
        {"termName", profile.term_name},
    });
}

Model::FeatureRecord course_to_record(const Model::BykcCourse &course) {
    return make_record(course.id, course.name, course.status, {
        {"teacher", course.teacher},
        {"position", course.position},
        {"startDate", course.start_date},
        {"endDate", course.end_date},
        {"selectStartDate", course.select_start_date},
        {"selectEndDate", course.select_end_date},
        {"cancelEndDate", course.cancel_end_date},
        {"maxCount", course.max_count},
        {"currentCount", course.current_count},
        {"category", course.category},
        {"subCategory", course.sub_category},
        {"selected", course.selected},
    });
}

Model::FeatureRecord detail_to_record(const Model::BykcCourseDetail &course) {
    return make_record(course.id, course.name, course.status, {
        {"teacher", course.teacher},
        {"position", course.position},
        {"contact", course.contact},
        {"mobile", course.mobile},
        {"desc", course.description},
        {"startDate", course.start_date},
        {"endDate", course.end_date},
        {"selected", course.selected},
        {"signConfig", course.sign_config},
    });
}

Model::FeatureRecord chosen_to_record(const Model::BykcChosenCourse &course) {
    return make_record(course.id, course.name, "selected", {
        {"courseId", course.course_id},
        {"teacher", course.teacher},
        {"position", course.position},
        {"selectDate", course.select_date},
        {"checkin", course.checkin},
        {"pass", course.pass},
        {"score", course.score},
        {"homework", course.homework},
        {"signInfo", course.sign_info},
    });
}

Model::FeatureRecord stat_to_record(const Model::BykcStat &stat) {
    std::map<std::string, std::string> fields;
    if (!stat.valid_count.empty()) fields["validCount"] = stat.valid_count;
    if (!stat.category.empty()) fields["category"] = stat.category;
    if (!stat.required_count.empty()) fields["requiredCount"] = stat.required_count;
    if (!stat.passed_count.empty()) fields["passedCount"] = stat.passed_count;
    return make_record(stat.id, stat.title, "ok", std::move(fields));
}

} // namespace

BykcService::BykcService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : BykcService(http_client, cache, mode, default_crypto_provider()) {}

BykcService::BykcService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto)
    : m_http_client(http_client), m_cache(cache), m_mode(mode), m_crypto(crypto) {}

Result<void> BykcService::ensure_login(bool force_refresh) {
    (void)m_cache;
    if (!force_refresh && !m_token.empty()) return {};

    auto token = acquire_bykc_token(m_http_client, m_mode, "https://bykc.buaa.edu.cn/sscv/cas/login", "博雅登录失败");
    if (!token) return make_error(token.error().code, token.error().message);
    if (token->empty()) {
        token = acquire_bykc_token(m_http_client, m_mode, "https://bykc.buaa.edu.cn/cas-login?token=", "博雅 CAS 登录失败");
        if (!token) return make_error(token.error().code, token.error().message);
    }
    if (token->empty()) {
        auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                     Protocol::DownstreamActivationStage::ArtifactExtract,
                                                     Protocol::DownstreamSessionState::TokenMissing,
                                                     "未获取到博雅 auth token");
        return make_error(error.code, Protocol::to_error(error).message);
    }
    m_token = *token;
    return {};
}

Result<std::string> BykcService::call_api_raw_once(const std::string &api_name, const nlohmann::json &payload) {
    auto plain = payload.dump();
    auto encrypted = encrypt_request(plain, m_crypto);
    if (!encrypted) return make_error(encrypted.error().code, encrypted.error().message);
    auto [body, ak, sk, aes_key] = *encrypted;

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode("https://bykc.buaa.edu.cn/sscv/" + api_name, m_mode);
    request.headers["Content-Type"] = "application/json; charset=UTF-8";
    request.headers["Accept"] = "application/json";
    request.headers["Referer"] = resolve_for_mode("https://bykc.buaa.edu.cn/system/course-select", m_mode);
    request.headers["Origin"] = resolve_for_mode("https://bykc.buaa.edu.cn", m_mode);
    request.headers["ak"] = ak;
    request.headers["sk"] = sk;
    request.headers["ts"] = std::to_string(now_millis());
    request.body = body;

    request.headers["auth_token"] = m_token;
    request.headers["authtoken"] = m_token;

    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, response.error().message);
    if (response_is_login(*response)) {
        auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                     Protocol::DownstreamActivationStage::Request,
                                                     Protocol::DownstreamSessionState::TokenExpired,
                                                     "博雅会话已过期，请重新登录");
        return make_error(error.code, Protocol::to_error(error).message);
    }
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "博雅请求返回: " + std::to_string(response->status_code));

    auto encoded_json = nlohmann::json::parse(response->body, nullptr, false);
    std::string encoded_payload = encoded_json.is_string() ? encoded_json.get<std::string>() : response->body;
    auto encrypted_response = base64_decode(encoded_payload);
    auto decrypted = m_crypto.aes_ecb_pkcs7_decrypt(encrypted_response, aes_key);
    std::string decoded = decrypted ? std::string(decrypted->begin(), decrypted->end()) : encoded_payload;
    if (decoded.find("会话已失效") != std::string::npos || decoded.find("未登录") != std::string::npos) {
        auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Bykc,
                                                     Protocol::DownstreamActivationStage::Request,
                                                     Protocol::DownstreamSessionState::TokenExpired,
                                                     "博雅会话已过期，请重新登录");
        return make_error(error.code, Protocol::to_error(error).message);
    }
    return decoded;
}

Result<std::string> BykcService::call_api_raw(const std::string &api_name, const nlohmann::json &payload, bool allow_retry) {
    auto login = ensure_login();
    if (!login) return make_error(login.error().code, login.error().message);

    auto raw = call_api_raw_once(api_name, payload);
    if (!raw && raw.error().code == ErrorCode::SessionExpired && allow_retry) {
        m_token.clear();
        auto refreshed = ensure_login(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        return call_api_raw(api_name, payload, false);
    }
    return raw;
}

Result<nlohmann::json> BykcService::call_api_data(const std::string &api_name, const nlohmann::json &payload, const std::string &fallback_message) {
    auto raw = call_api_raw(api_name, payload);
    if (!raw) return make_error(raw.error().code, raw.error().message);
    auto json = nlohmann::json::parse(*raw, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析博雅 JSON 失败");
    bool success = false;
    if (json.contains("success") && json["success"].is_boolean()) {
        success = json["success"].get<bool>();
    } else if (json.contains("isSuccess") && json["isSuccess"].is_boolean()) {
        success = json["isSuccess"].get<bool>();
    } else {
        auto status = json_string(json, "status");
        success = status == "0";
    }
    if (!success || !json.contains("data") || json["data"].is_null()) {
        auto message = json_string(json, "errmsg");
        if (message.empty()) message = json_string(json, "msg");
        if (message.empty()) message = fallback_message;
        return make_error(ErrorCode::NetworkError, message);
    }
    return json["data"];
}

Result<Model::BykcProfile> BykcService::get_profile() {
    auto data = call_api_data("getUserProfile", nlohmann::json::object(), "博雅资料加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_bykc_profile(*data);
}

Result<std::vector<Model::FeatureRecord>> BykcService::profile() {
    auto result = get_profile();
    if (!result) return make_error(result.error().code, result.error().message);
    return std::vector<Model::FeatureRecord>{profile_to_record(*result)};
}

Result<std::vector<Model::BykcCourse>> BykcService::list_courses(int page, int size, bool all) {
    BykcCourseQuery query;
    query.page = page;
    query.size = size;
    query.all = all;
    return list_courses(query);
}

Result<std::vector<Model::BykcCourse>> BykcService::list_courses(const BykcCourseQuery &query) {
    nlohmann::json payload{{"pageNumber", query.page < 1 ? 1 : query.page}, {"pageSize", query.size < 1 ? 100 : query.size}, {"all", query.all}};
    if (!query.status.empty()) payload["status"] = query.status;
    if (!query.category.empty()) payload["category"] = query.category;
    if (!query.sub_category.empty()) payload["subCategory"] = query.sub_category;
    if (!query.campus.empty()) payload["campus"] = query.campus;
    if (!query.keyword.empty()) payload["keyword"] = query.keyword;

    auto data = call_api_data("queryStudentSemesterCourseByPage", payload, "博雅课程列表加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    auto content = data->contains("content") && (*data)["content"].is_array() ? (*data)["content"] : nlohmann::json::array();
    auto courses = Parser::parse_bykc_courses(content);
    std::vector<Model::BykcCourse> records;
    for (auto course : courses) {
        if (!query.status.empty() && course.status != query.status) continue;
        if (!query.category.empty() && course.category.find(query.category) == std::string::npos) continue;
        if (!query.sub_category.empty() && course.sub_category.find(query.sub_category) == std::string::npos) continue;
        if (!query.campus.empty() && course.position.find(query.campus) == std::string::npos) continue;
        if (!query.keyword.empty() && course.name.find(query.keyword) == std::string::npos && course.teacher.find(query.keyword) == std::string::npos) continue;
        records.push_back(std::move(course));
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> BykcService::courses(int page, int size, bool all) {
    BykcCourseQuery query;
    query.page = page;
    query.size = size;
    query.all = all;
    return courses(query);
}

Result<std::vector<Model::FeatureRecord>> BykcService::courses(const BykcCourseQuery &query) {
    auto result = list_courses(query);
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &course : *result) records.push_back(course_to_record(course));
    return records;
}

Result<Model::BykcCourseDetail> BykcService::course_detail(const std::string &course_id) {
    auto id = parse_positive_id(course_id, "bykc course id");
    if (!id) return make_error(id.error().code, id.error().message);
    auto data = call_api_data("queryCourseById", nlohmann::json{{"id", *id}}, "博雅课程详情加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_bykc_course_detail(*data, course_id);
}

Result<Model::FeatureRecord> BykcService::show_course(const std::string &course_id) {
    auto result = course_detail(course_id);
    if (!result) return make_error(result.error().code, result.error().message);
    return detail_to_record(*result);
}

Result<std::vector<Model::BykcChosenCourse>> BykcService::list_chosen_courses() {
    auto config = call_api_data("getAllConfig", nlohmann::json::object(), "博雅配置加载失败");
    if (!config) return make_error(config.error().code, config.error().message);
    auto semesters = config->contains("semester") && (*config)["semester"].is_array() ? (*config)["semester"] : nlohmann::json::array();
    if (semesters.empty()) return make_error(ErrorCode::ParseError, "无法获取当前博雅学期信息");
    auto semester = semesters.back();
    auto start = json_string(semester, "semesterStartDate");
    auto end = json_string(semester, "semesterEndDate");
    auto data = call_api_data("queryChosenCourse", nlohmann::json{{"startDate", start}, {"endDate", end}}, "博雅已选课程加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    auto list = data->contains("courseList") && (*data)["courseList"].is_array() ? (*data)["courseList"] : nlohmann::json::array();
    return Parser::parse_bykc_chosen_courses(list);
}

Result<std::vector<Model::FeatureRecord>> BykcService::chosen() {
    auto result = list_chosen_courses();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &course : *result) records.push_back(chosen_to_record(course));
    return records;
}

Result<std::vector<Model::BykcStat>> BykcService::list_stats() {
    auto data = call_api_data("queryStatisticByUserId", nlohmann::json::object(), "博雅统计加载失败");
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_bykc_stats(*data);
}

Result<std::vector<Model::FeatureRecord>> BykcService::stats() {
    auto result = list_stats();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &stat : *result) records.push_back(stat_to_record(stat));
    return records;
}

Result<Model::MutationResult> BykcService::select_course(const std::string &course_id) {
    auto id = parse_positive_id(course_id, "bykc course id");
    if (!id) return make_error(id.error().code, id.error().message);
    auto data = call_api_data("choseCourse", nlohmann::json{{"courseId", *id}}, "选课失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = "选课成功";
    result.summary = make_record(course_id, "博雅选课", "selected", {{"raw", data->dump()}});
    return result;
}

Result<Model::MutationResult> BykcService::unselect_course(const std::string &course_id) {
    auto id = parse_positive_id(course_id, "bykc chosen id");
    if (!id) return make_error(id.error().code, id.error().message);
    auto data = call_api_data("delChosenCourse", nlohmann::json{{"id", *id}}, "退选失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = "退选成功";
    result.summary = make_record(course_id, "博雅退选", "unselected", {{"raw", data->dump()}});
    return result;
}

Result<Model::MutationResult> BykcService::sign_course(const std::string &course_id, int sign_type) {
    auto id = parse_positive_id(course_id, "bykc course id");
    if (!id) return make_error(id.error().code, id.error().message);
    if (sign_type != 1 && sign_type != 2) return make_error(ErrorCode::InvalidArgument, "bykc sign 需要 --sign-type 1 或 2");
    auto detail = course_detail(course_id);
    if (!detail) return make_error(detail.error().code, detail.error().message);
    auto location = random_sign_location_from_config(detail->sign_config);
    if (!location) return make_error(location.error().code, location.error().message);
    auto data = call_api_data("signCourseByUser", nlohmann::json{{"courseId", *id}, {"signLat", location->lat}, {"signLng", location->lng}, {"signType", sign_type}}, sign_type == 1 ? "签到失败" : "签退失败");
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = sign_type == 1 ? "签到成功" : "签退成功";
    result.summary = make_record(course_id, sign_type == 1 ? "博雅签到" : "博雅签退", sign_type == 1 ? "signed" : "signed-out", {{"raw", data->dump()}});
    return result;
}

} // namespace UBAANext
