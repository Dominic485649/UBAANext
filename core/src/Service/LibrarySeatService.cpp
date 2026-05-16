#include <UBAANext/Service/LibrarySeatService.hpp>

#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/LibrarySeatParser.hpp>

#include <ctime>
#include <iomanip>
#include <initializer_list>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace UBAANext {

namespace {

constexpr const char *base_url = "https://booking.lib.buaa.edu.cn";
constexpr const char *cas_login_url = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbooking.lib.buaa.edu.cn%2Fv4%2Flogin%2Fcas";
constexpr const char *reserve_iv = "ZZWBKJ_ZHIHUAWEI";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    for (const auto &[key, value] : response.headers) {
        if (key.size() != name.size()) {
            continue;
        }
        bool same = true;
        for (size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) != std::tolower(static_cast<unsigned char>(name[i]))) {
                same = false;
                break;
            }
        }
        if (same) {
            auto newline = value.find('\n');
            return newline == std::string::npos ? value : value.substr(0, newline);
        }
    }
    return {};
}

std::string resolve_redirect_url(const std::string &base, const std::string &location) {
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
        return location;
    }
    std::regex url_re(R"(^([^:]+://[^/]+)(/.*)?$)");
    std::smatch match;
    if (!std::regex_search(base, match, url_re)) {
        return location;
    }
    std::string authority = match[1].str();
    if (!location.empty() && location.front() == '/') {
        return authority + location;
    }
    return authority + "/" + location;
}

std::string extract_cas_token(const std::string &url) {
    std::regex token_re(R"([?&#]cas=([^&#]+))");
    std::smatch match;
    return std::regex_search(url, match, token_re) && match.size() > 1 ? match[1].str() : std::string{};
}

std::string today_yyyy_mm_dd() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

std::string libbook_aes_key(const std::string &day) {
    std::string digits;
    for (char ch : day) {
        if (ch >= '0' && ch <= '9') digits.push_back(ch);
    }
    if (digits.size() != 8) return {};
    std::string reversed(digits.rbegin(), digits.rend());
    return digits + reversed;
}

Result<std::string> encrypt_reserve_payload(const nlohmann::json &body, const std::string &day) {
    auto key = libbook_aes_key(day);
    if (key.empty()) return make_error(ErrorCode::InvalidArgument, "libbook book 需要有效 --date <yyyy-MM-dd>");
    auto plain = body.dump();
    std::vector<unsigned char> data(plain.begin(), plain.end());
    auto pad = 16 - (data.size() % 16);
    data.insert(data.end(), pad, static_cast<unsigned char>(pad));
    auto encrypted = default_crypto_provider().aes_cbc_encrypt(data, key, reserve_iv);
    if (!encrypted) return make_error(encrypted.error().code, "LibBook " + encrypted.error().message);
    return base64_encode(*encrypted);
}

std::optional<std::pair<std::string, std::string>> parse_time_range(const std::string &segment) {
    auto separator = segment.find('-');
    if (separator == std::string::npos) separator = segment.find('~');
    if (separator == std::string::npos) return std::nullopt;
    auto start = segment.substr(0, separator);
    auto end = segment.substr(separator + 1);
    if (start.size() != 5 || end.size() != 5 || start[2] != ':' || end[2] != ':') return std::nullopt;
    return std::make_pair(start, end);
}

void apply_headers(HttpRequest &request) {
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["Referer"] = base_url;
    request.headers["Origin"] = base_url;
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) {
        return {};
    }
    if (json[key].is_string()) {
        return json[key].get<std::string>();
    }
    if (json[key].is_number_integer()) {
        return std::to_string(json[key].get<long long>());
    }
    return {};
}

bool contains_any(const std::string &text, std::initializer_list<std::string_view> needles) {
    for (auto needle : needles) {
        if (text.find(needle) != std::string::npos) return true;
    }
    return false;
}

bool libbook_business_success(const nlohmann::json &json) {
    auto code = json_string(json, "code");
    if (!code.empty() && code != "0" && code != "1") return false;
    auto message = json_string(json, "message");
    if (message.empty()) message = json_string(json, "msg");
    return !contains_any(message, {"失败", "不可", "已被", "不能取消", "无法取消", "已取消", "用户取消", "已结束", "已完成"});
}

Model::FeatureRecord make_record(std::string id,
                                 std::string title,
                                 std::string status,
                                 std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord to_record(const Model::LibraryInfo &library) {
    return make_record(library.id, library.name, "available", {{"freeNum", library.free_num}, {"totalNum", library.total_num}});
}

Model::FeatureRecord to_record(const Model::LibraryArea &area) {
    return make_record(area.id,
                       area.name,
                       "available",
                       {{"area", area.area}, {"premisesId", area.premises_id}, {"storeyId", area.storey_id}, {"freeNum", area.free_num}, {"totalNum", area.total_num}, {"availableDates", area.available_dates}});
}

Model::FeatureRecord to_record(const Model::LibrarySeat &seat) {
    return make_record(seat.id, seat.title, seat.status, {{"name", seat.name}, {"status", seat.raw_status}, {"statusName", seat.status_name}});
}

Model::FeatureRecord to_record(const Model::LibraryReservation &reservation) {
    return make_record(reservation.id,
                       reservation.title,
                       reservation.status,
                       {{"areaName", reservation.area_name}, {"day", reservation.day}, {"beginTime", reservation.begin_time}, {"endTime", reservation.end_time}, {"statusName", reservation.status_name}});
}

template <typename T>
std::vector<Model::FeatureRecord> to_records(const std::vector<T> &items) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(items.size());
    for (const auto &item : items) records.push_back(to_record(item));
    return records;
}

} // namespace

LibrarySeatService::LibrarySeatService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::string> LibrarySeatService::fetch_cas_token() {
    std::string current_url = cas_login_url;
    for (int i = 0; i < 8; ++i) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, m_mode);
        apply_headers(request);
        auto response = m_http_client.send(request);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "获取图书馆 CAS 参数失败: " + response.error().message);
        }

        if (auto token = extract_cas_token(current_url); !token.empty()) {
            return token;
        }
        auto location = header_value(*response, "Location");
        if (auto token = extract_cas_token(location); !token.empty()) {
            return token;
        }
        if (response->status_code < 300 || response->status_code >= 400 || location.empty()) {
            break;
        }
        current_url = resolve_redirect_url(current_url, location);
    }
    return make_error(ErrorCode::SessionExpired, "未能获取图书馆 CAS 参数，请重新登录");
}

Result<void> LibrarySeatService::ensure_login(bool force_refresh) {
    if (!force_refresh && !m_token.empty()) {
        return {};
    }

    auto cas = fetch_cas_token();
    if (!cas) {
        return make_error(cas.error().code, cas.error().message);
    }

    auto response = request_json("login/user", {{"cas", *cas}}, false, false);
    if (!response) {
        return make_error(response.error().code, response.error().message);
    }

    try {
        auto member = (*response)["data"]["member"];
        auto token = json_string(member, "token");
        if (token.empty()) {
            return make_error(ErrorCode::SessionExpired, "图书馆登录成功但未返回 token");
        }
        m_token = std::move(token);
        return {};
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析图书馆登录响应失败: ") + e.what());
    }
}

Result<nlohmann::json> LibrarySeatService::request_json(const std::string &path, const nlohmann::json &body, bool authorize, bool allow_retry) {
    if (authorize) {
        auto login = ensure_login();
        if (!login) {
            return make_error(login.error().code, login.error().message);
        }
    }

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(std::string(base_url) + "/v4/" + path, m_mode);
    apply_headers(request);
    request.headers["Content-Type"] = "application/json";
    if (authorize) {
        request.headers["Authorization"] = "bearer" + m_token;
    }
    request.body = body.dump();

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求图书馆座位系统失败: " + response.error().message);
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "图书馆座位系统返回: " + std::to_string(response->status_code));
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        auto message = json_string(json, "message");
        if (message.find("登录失效") != std::string::npos || message.find("请重新登录") != std::string::npos || message.find("未登录") != std::string::npos) {
            m_token.clear();
            if (authorize && allow_retry) {
                auto login = ensure_login(true);
                if (!login) {
                    return make_error(login.error().code, login.error().message);
                }
                return request_json(path, body, authorize, false);
            }
            return make_error(ErrorCode::SessionExpired, "图书馆登录状态已失效");
        }
        if (!libbook_business_success(json)) {
            return make_error(ErrorCode::NetworkError, message.empty() ? "图书馆接口请求失败" : message);
        }
        return json;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析图书馆座位 JSON 失败: ") + e.what());
    }
}

Result<std::vector<Model::LibraryInfo>> LibrarySeatService::libraries(const std::string &day) {
    (void)m_cache;
    auto json = request_json("space/pcTopFor", {{"day", day.empty() ? today_yyyy_mm_dd() : day}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }
    return Parser::parse_library_infos((*json)["data"]["list"]);
}

Result<std::vector<Model::LibraryArea>> LibrarySeatService::areas(const std::string &library_id, const std::string &day, const std::string &storey_id) {
    if (library_id.empty() || library_id == "areas") {
        return make_error(ErrorCode::InvalidArgument, "libbook areas 需要 --library-id <id>");
    }
    auto storeys = nlohmann::json::array();
    if (!storey_id.empty()) {
        storeys.push_back(storey_id);
    }
    auto json = request_json("space/pick", {{"premisesIds", library_id}, {"categoryIds", nlohmann::json::array()}, {"storeyIds", storeys}, {"boutiqueIds", nlohmann::json::array()}, {"date", day.empty() ? today_yyyy_mm_dd() : day}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }
    return Parser::parse_library_areas((*json)["data"]["area"]);
}

Result<Model::LibraryArea> LibrarySeatService::area_detail(const std::string &area_id) {
    if (area_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "libbook area show 需要 --area-id <id>");
    }
    auto json = request_json("Space/map", {{"id", area_id}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }
    return Parser::parse_library_area_detail((*json)["data"], area_id);
}

Result<std::vector<Model::LibrarySeat>> LibrarySeatService::seats(const std::string &area_id, const std::string &day, const std::string &start_time, const std::string &end_time) {
    if (area_id.empty() || area_id == "seats") {
        return make_error(ErrorCode::InvalidArgument, "libbook seats 需要 --area-id <id>");
    }
    auto json = request_json("Space/seat", {{"id", area_id}, {"day", day.empty() ? today_yyyy_mm_dd() : day}, {"label_id", nlohmann::json::array()}, {"start_time", start_time.empty() ? "08:00" : start_time}, {"end_time", end_time.empty() ? "22:00" : end_time}, {"begdate", ""}, {"enddate", ""}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }
    return Parser::parse_library_seats((*json)["data"]["list"]);
}

Result<std::vector<Model::LibraryReservation>> LibrarySeatService::reservations(int page, int limit) {
    auto json = request_json("member/seat", {{"type", "1"}, {"page", page < 1 ? 1 : page}, {"limit", limit < 1 ? 20 : limit}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }
    auto data = (*json)["data"];
    return Parser::parse_library_reservations(data.contains("data") ? data["data"] : data["list"]);
}

Result<std::vector<Model::FeatureRecord>> LibrarySeatService::list_libraries(const std::string &day) {
    auto records = libraries(day);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<std::vector<Model::FeatureRecord>> LibrarySeatService::list_areas(const std::string &library_id, const std::string &day, const std::string &storey_id) {
    auto records = areas(library_id, day, storey_id);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<Model::FeatureRecord> LibrarySeatService::show_area(const std::string &area_id) {
    auto area = area_detail(area_id);
    if (!area) return make_error(area.error().code, area.error().message);
    return to_record(*area);
}

Result<std::vector<Model::FeatureRecord>> LibrarySeatService::list_seats(const std::string &area_id, const std::string &day, const std::string &start_time, const std::string &end_time) {
    auto records = seats(area_id, day, start_time, end_time);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<std::vector<Model::FeatureRecord>> LibrarySeatService::list_reservations(int page, int limit) {
    auto records = reservations(page, limit);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<Model::MutationResult> LibrarySeatService::reserve_seat(const std::string &seat_id,
                                                               const std::string &day,
                                                               const std::string &segment,
                                                               const std::string &start_time,
                                                               const std::string &end_time) {
    if (seat_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "libbook book 需要 --seat-id <id>");
    }
    if (day.empty()) {
        return make_error(ErrorCode::InvalidArgument, "libbook book 需要 --date <yyyy-MM-dd>");
    }
    if (segment.empty() && (start_time.empty() || end_time.empty())) {
        return make_error(ErrorCode::InvalidArgument, "libbook book 需要 --segment <segment> 或 --start-time/--end-time");
    }

    std::string resolved_start = start_time;
    std::string resolved_end = end_time;
    if ((resolved_start.empty() || resolved_end.empty()) && !segment.empty()) {
        if (auto range = parse_time_range(segment)) {
            resolved_start = range->first;
            resolved_end = range->second;
        }
    }

    auto encrypted = encrypt_reserve_payload({{"seat_id", seat_id}, {"segment", segment}, {"day", day}, {"start_time", resolved_start}, {"end_time", resolved_end}}, day);
    if (!encrypted) {
        return make_error(encrypted.error().code, encrypted.error().message);
    }
    auto json = request_json("space/confirm", {{"aesjson", *encrypted}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }

    auto message = json_string(*json, "message");
    if (message.empty()) message = json_string(*json, "msg");
    if (!libbook_business_success(*json)) {
        return make_error(ErrorCode::NetworkError, message.empty() ? "图书馆座位预约失败" : message);
    }
    if (message.empty()) message = "预约成功";

    Model::MutationResult result;
    result.accepted = true;
    result.message = message;
    result.summary = make_record(seat_id, "图书馆座位预约", "reserved", {{"day", day}, {"segment", segment}, {"startTime", resolved_start}, {"endTime", resolved_end}, {"raw", json->dump()}});
    return result;
}

Result<Model::MutationResult> LibrarySeatService::cancel_booking(const std::string &booking_id) {
    if (booking_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "libbook cancel 需要 --booking-id <id>");
    }

    auto json = request_json("space/cancel", {{"id", booking_id}});
    if (!json) {
        return make_error(json.error().code, json.error().message);
    }

    auto message = json_string(*json, "message");
    if (message.empty()) {
        message = json_string(*json, "msg");
    }
    if (!libbook_business_success(*json)) {
        return make_error(ErrorCode::NetworkError, message.empty() ? "图书馆预约取消失败" : message);
    }
    if (message.empty()) {
        message = "取消成功";
    }

    Model::MutationResult result;
    result.accepted = true;
    result.message = message;
    result.summary = make_record(booking_id, "图书馆预约", "cancelled", {{"bookingId", booking_id}});
    return result;
}

} // namespace UBAANext
