#include <UBAANext/Service/YgdkService.hpp>

#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/YgdkParser.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

namespace UBAANext {

namespace {

constexpr const char *default_place = "操场";
constexpr const char *transparent_png_base64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+lmCcAAAAASUVORK5CYII=";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string url_encode(const std::string &value) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') out << static_cast<char>(ch);
        else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
    return out.str();
}

std::string url_decode(std::string value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            auto hex = value.substr(i + 1, 2);
            char *end = nullptr;
            auto decoded = std::strtol(hex.c_str(), &end, 16);
            if (end && *end == '\0') {
                out.push_back(static_cast<char>(decoded));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return out;
}

std::string form_encode(const std::map<std::string, std::string> &form) {
    std::string body;
    for (const auto &[key, value] : form) {
        if (!body.empty()) body += '&';
        body += url_encode(key) + "=" + url_encode(value);
    }
    return body;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    for (const auto &[key, value] : response.headers) {
        if (key.size() != name.size()) continue;
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
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) return location;
    std::regex url_re(R"(^([^:]+://[^/]+)(/.*)?$)");
    std::smatch match;
    if (!std::regex_search(base, match, url_re)) return location;
    std::string authority = match[1].str();
    std::string path = match.size() > 2 ? match[2].str() : "/";
    if (location.rfind("//", 0) == 0) {
        auto colon = authority.find(':');
        return authority.substr(0, colon) + ":" + location;
    }
    if (!location.empty() && location.front() == '/') return authority + location;
    auto slash = path.find_last_of('/');
    return authority + (slash == std::string::npos ? "/" : path.substr(0, slash + 1)) + location;
}

bool response_is_login(const HttpResponse &response) {
    return response.status_code == 401 || response.status_code == 403 ||
           response.body.find("name=\"execution\"") != std::string::npos ||
           response.body.find("统一身份认证") != std::string::npos;
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    return {};
}

int json_int(const nlohmann::json &json, const char *key, int fallback = 0) {
    if (!json.contains(key) || json[key].is_null()) return fallback;
    if (json[key].is_number_integer()) return json[key].get<int>();
    if (json[key].is_string()) {
        try { return std::stoi(json[key].get<std::string>()); } catch (...) { return fallback; }
    }
    return fallback;
}

nlohmann::json unwrap_response(const std::string &body) {
    auto payload = nlohmann::json::parse(body, nullptr, false);
    if (payload.is_discarded()) return nlohmann::json{{"__error", "阳光打卡返回无法解析"}};
    auto code = json_int(payload, "code", 0);
    if (code == 1) {
        if (payload.contains("result")) return payload["result"];
        return nlohmann::json::object();
    }
    if (code == -98) return nlohmann::json{{"__session_expired", true}};
    return nlohmann::json{{"__error", payload.value("msg", std::string("阳光打卡请求失败"))}};
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord overview_to_record(const Model::YgdkOverview &overview) {
    return make_record(overview.classify.id, overview.classify.name, "available", {
        {"termName", overview.term_name},
        {"termCount", overview.term_count},
        {"termGoodCount", overview.term_good_count},
        {"weekCount", overview.week_count},
        {"monthCount", overview.month_count},
        {"dayCount", overview.day_count},
    });
}

Model::FeatureRecord item_to_record(const Model::YgdkItem &item) {
    return make_record(item.id, item.name, "item", {{"classifyId", item.classify_id}, {"sort", item.sort}});
}

Model::FeatureRecord record_to_record(const Model::YgdkRecord &record) {
    return make_record(record.id, record.item_name, record.state, {
        {"place", record.place},
        {"startTime", record.start_time},
        {"endTime", record.end_time},
        {"createdAt", record.created_at},
    });
}

std::string mime_for_file(const std::filesystem::path &path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".webp") return "image/webp";
    return "application/octet-stream";
}

Result<std::string> read_binary_file(const std::string &path_text) {
    std::ifstream input(path_text, std::ios::binary);
    if (!input) return make_error(ErrorCode::InvalidArgument, "无法读取图片文件: " + path_text);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string default_transparent_photo() {
    auto bytes = base64_decode(transparent_png_base64);
    return std::string(bytes.begin(), bytes.end());
}

std::string trim_copy(const std::string &value) {
    auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

struct ClockinTimeRange {
    std::string start_epoch;
    std::string end_epoch;
    std::string formatted;
};

constexpr long long shanghai_offset_seconds = 8 * 60 * 60;

std::tm utc_tm(std::time_t value) {
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &value);
#else
    gmtime_r(&value, &utc);
#endif
    return utc;
}

std::tm shanghai_tm(std::time_t value) {
    return utc_tm(value + shanghai_offset_seconds);
}

long long days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097LL + static_cast<long long>(day_of_era) - 719468;
}

std::time_t shanghai_epoch_from_tm(const std::tm &value) {
    const int year = value.tm_year + 1900;
    const auto month = static_cast<unsigned>(value.tm_mon + 1);
    const auto day = static_cast<unsigned>(value.tm_mday);
    auto seconds = days_from_civil(year, month, day) * 24 * 60 * 60;
    seconds += value.tm_hour * 60 * 60 + value.tm_min * 60 + value.tm_sec;
    seconds -= shanghai_offset_seconds;
    return static_cast<std::time_t>(seconds);
}

std::string format_date_time(const std::tm &value) {
    std::ostringstream out;
    out << std::put_time(&value, "%Y-%m-%d %H:%M");
    return out.str();
}

std::string format_hour_minute(const std::tm &value) {
    std::ostringstream out;
    out << std::put_time(&value, "%H:%M");
    return out.str();
}

std::string format_clockin_time_range(std::time_t start, std::time_t end) {
    auto start_local = shanghai_tm(start);
    auto end_local = shanghai_tm(end);
    return format_date_time(start_local) + "-" + format_hour_minute(end_local);
}

Result<std::time_t> parse_clockin_time(const std::string &value) {
    std::tm parsed{};
    parsed.tm_isdst = -1;
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M");
    if (input.fail() || !input.eof()) return make_error(ErrorCode::InvalidArgument, "时间格式错误，请使用 yyyy-MM-dd HH:mm");
    return shanghai_epoch_from_tm(parsed);
}

ClockinTimeRange default_clockin_time_range() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    auto local = shanghai_tm(now_time);
    bool use_previous_day = local.tm_hour < 9;
    int start_hour = std::clamp(local.tm_hour - 1, 8, 21);
    if (use_previous_day) {
        now_time -= 24 * 60 * 60;
        local = shanghai_tm(now_time);
        start_hour = 20;
    }
    local.tm_hour = start_hour;
    local.tm_min = 0;
    local.tm_sec = 0;
    auto start = shanghai_epoch_from_tm(local);
    auto end = start + 60 * 60;
    return {std::to_string(start), std::to_string(end), format_clockin_time_range(start, end)};
}

Result<ClockinTimeRange> resolve_clockin_time_range(const std::string &start_time, const std::string &end_time) {
    auto normalized_start = trim_copy(start_time);
    auto normalized_end = trim_copy(end_time);
    if ((normalized_start.empty() && !normalized_end.empty()) || (!normalized_start.empty() && normalized_end.empty())) return make_error(ErrorCode::InvalidArgument, "ygdk submit 需要同时提供 --start-time 和 --end-time");
    if (normalized_start.empty() && normalized_end.empty()) return default_clockin_time_range();

    auto start = parse_clockin_time(normalized_start);
    if (!start) return make_error(start.error().code, start.error().message);
    auto end = parse_clockin_time(normalized_end);
    if (!end) return make_error(end.error().code, end.error().message);

    auto start_local = shanghai_tm(*start);
    auto end_local = shanghai_tm(*end);
    if (start_local.tm_year != end_local.tm_year || start_local.tm_yday != end_local.tm_yday) return make_error(ErrorCode::InvalidArgument, "当前仅支持同一天内的一小时打卡");
    if (*end <= *start) return make_error(ErrorCode::InvalidArgument, "结束时间必须晚于开始时间");

    return ClockinTimeRange{std::to_string(*start), std::to_string(*end), format_clockin_time_range(*start, *end)};
}

int item_sort_value(const Model::YgdkItem &item) {
    try {
        return item.sort.empty() ? std::numeric_limits<int>::max() : std::stoi(item.sort);
    } catch (...) {
        return std::numeric_limits<int>::max();
    }
}

Result<Model::YgdkItem> select_clockin_item(const std::vector<Model::YgdkItem> &items, const std::string &requested_item_id) {
    auto normalized_item_id = trim_copy(requested_item_id);
    if (items.empty()) return make_error(ErrorCode::ParseError, "未获取到阳光打卡项目列表");
    if (!normalized_item_id.empty()) {
        for (const auto &item : items) {
            if (item.id == normalized_item_id) return item;
        }
        return make_error(ErrorCode::InvalidArgument, "所选阳光打卡项目不存在: " + normalized_item_id);
    }
    for (const auto &item : items) {
        if (item.name.find("跑") != std::string::npos) return item;
    }
    return *std::min_element(items.begin(), items.end(), [](const auto &left, const auto &right) {
        return item_sort_value(left) < item_sort_value(right);
    });
}

void append_multipart_field(std::string &body, const std::string &boundary, const std::string &name, const std::string &value) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
    body += value + "\r\n";
}

void append_multipart_file(std::string &body, const std::string &boundary, const std::string &name, const std::filesystem::path &path, const std::string &content) {
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"" + name + "\"; filename=\"" + path.filename().string() + "\"\r\n";
    body += "Content-Type: " + mime_for_file(path) + "\r\n\r\n";
    body += content;
    body += "\r\n";
}

} // namespace

YgdkService::YgdkService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::string> YgdkService::fetch_oauth_code() {
    std::string current_url = "https://app.buaa.edu.cn/uc/api/oauth/index?redirect=https%3A%2F%2Fygdk.buaa.edu.cn%2F%23%2Fhome&appid=200230221144501510&state=STATE&qrcode=1";
    std::regex code_re(R"([?&]code=([^&#]+))");
    for (int i = 0; i < 10; ++i) {
        std::smatch match;
        if (std::regex_search(current_url, match, code_re) && match.size() > 1) return url_decode(match[1].str());
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, m_mode);
        request.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
        auto response = m_http_client.send(request);
        if (!response) return make_error(ErrorCode::NetworkError, "获取阳光打卡 OAuth code 失败: " + response.error().message);
        if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "阳光打卡会话已过期，请重新登录");
        auto location = header_value(*response, "Location");
        if (std::regex_search(location, match, code_re) && match.size() > 1) return url_decode(match[1].str());
        if (location.empty()) return make_error(ErrorCode::NetworkError, "阳光打卡 OAuth 跳转缺少 Location");
        current_url = resolve_redirect_url(current_url, location);
    }
    return make_error(ErrorCode::NetworkError, "无法获取阳光打卡登录 code");
}

Result<void> YgdkService::ensure_session(bool force_refresh) {
    (void)m_cache;
    if (!force_refresh && !m_uid.empty() && !m_token.empty()) return {};
    auto code = fetch_oauth_code();
    if (!code) return make_error(code.error().code, code.error().message);

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://ygdk.buaa.edu.cn/api/Front/Clockin/User/campusAppLogin?code=" + url_encode(*code), m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "阳光打卡登录失败: " + response.error().message);
    if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "阳光打卡会话已过期，请重新登录");
    auto data = unwrap_response(response->body);
    if (data.contains("__session_expired")) return make_error(ErrorCode::SessionExpired, "阳光打卡会话已过期，请重新登录");
    if (data.contains("__error")) return make_error(ErrorCode::NetworkError, data["__error"].get<std::string>());
    auto payload = data.contains("data") && data["data"].is_object() ? data["data"] : data;
    m_uid = json_string(payload, "uid");
    m_token = url_decode(json_string(payload, "token"));
    if (m_uid.empty() || m_token.empty()) return make_error(ErrorCode::ParseError, "阳光打卡登录响应缺少 uid/token");
    return {};
}

Result<nlohmann::json> YgdkService::post_form(const std::string &url, const std::map<std::string, std::string> &query, const std::map<std::string, std::string> &form) {
    auto login = ensure_session();
    if (!login) return make_error(login.error().code, login.error().message);
    auto all_form = form;
    all_form["uid"] = m_uid;
    all_form["token"] = m_token;
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(url + (query.empty() ? "" : "?" + form_encode(query)), m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.body = form_encode(all_form);
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "请求阳光打卡失败: " + response.error().message);
    auto data = unwrap_response(response->body);
    if (data.contains("__session_expired")) {
        auto refreshed = ensure_session(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        return post_form(url, query, form);
    }
    if (data.contains("__error")) return make_error(ErrorCode::NetworkError, data["__error"].get<std::string>());
    return data;
}

Result<std::pair<Model::YgdkOverview, std::vector<Model::YgdkItem>>> YgdkService::overview_data() {
    auto classifies = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Classify/getList");
    if (!classifies) return make_error(classifies.error().code, classifies.error().message);
    auto classify_list = Parser::parse_ygdk_classifies(*classifies);
    if (classify_list.empty()) return make_error(ErrorCode::ParseError, "未获取到阳光打卡分类");
    auto classify = Parser::select_ygdk_sports_classify(classify_list);
    auto items = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Item/getList", {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify.id}}, {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify.id}});
    auto count = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/getCount", {}, {{"classify_id", classify.id}, {"user_id", m_uid}});
    auto term = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Term/get");
    return std::make_pair(Parser::parse_ygdk_overview(classify, term ? &*term : nullptr, count ? &*count : nullptr),
                          items ? Parser::parse_ygdk_items(*items, classify.id) : std::vector<Model::YgdkItem>{});
}

Result<std::vector<Model::FeatureRecord>> YgdkService::overview() {
    auto data = overview_data();
    if (!data) return make_error(data.error().code, data.error().message);
    std::vector<Model::FeatureRecord> records;
    records.push_back(overview_to_record(data->first));
    for (const auto &item : data->second) records.push_back(item_to_record(item));
    return records;
}

Result<std::vector<Model::YgdkRecord>> YgdkService::record_list(int page, int size) {
    auto classifies = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Classify/getList");
    if (!classifies) return make_error(classifies.error().code, classifies.error().message);
    auto classify_list = Parser::parse_ygdk_classifies(*classifies);
    if (classify_list.empty()) return make_error(ErrorCode::ParseError, "未获取到阳光打卡分类");
    auto classify_id = classify_list.front().id;
    auto page_text = std::to_string(page < 1 ? 1 : page);
    auto size_text = std::to_string(size < 1 ? 20 : size);
    auto data = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/getList", {{"page", page_text}, {"limit", size_text}, {"classify_id", classify_id}, {"user_id", m_uid}}, {{"page", page_text}, {"limit", size_text}, {"classify_id", classify_id}, {"user_id", m_uid}});
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_ygdk_records(*data);
}

Result<std::vector<Model::FeatureRecord>> YgdkService::records(int page, int size) {
    auto result = record_list(page, size);
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &record : *result) records.push_back(record_to_record(record));
    return records;
}

Result<Model::MutationResult> YgdkService::submit_clockin(const std::string &item_id,
                                                          const std::string &start_time,
                                                          const std::string &end_time,
                                                          const std::string &place,
                                                          bool share_to_square,
                                                          const std::string &photo_path) {
    auto resolved_time = resolve_clockin_time_range(start_time, end_time);
    if (!resolved_time) return make_error(resolved_time.error().code, resolved_time.error().message);
    auto normalized_place = trim_copy(place);
    auto resolved_place = normalized_place.empty() ? std::string(default_place) : normalized_place;

    auto login = ensure_session();
    if (!login) return make_error(login.error().code, login.error().message);

    auto classifies = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Classify/getList");
    if (!classifies) return make_error(classifies.error().code, classifies.error().message);
    auto classify_list = Parser::parse_ygdk_classifies(*classifies);
    if (classify_list.empty()) return make_error(ErrorCode::ParseError, "未获取到阳光打卡分类");
    auto classify = Parser::select_ygdk_sports_classify(classify_list);
    auto items = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Item/getList", {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify.id}}, {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify.id}});
    if (!items) return make_error(items.error().code, items.error().message);
    auto selected_item = select_clockin_item(Parser::parse_ygdk_items(*items, classify.id), item_id);
    if (!selected_item) return make_error(selected_item.error().code, selected_item.error().message);

    auto file_content = photo_path.empty() ? Result<std::string>(default_transparent_photo()) : read_binary_file(photo_path);
    if (!file_content) return make_error(file_content.error().code, file_content.error().message);
    auto path = photo_path.empty() ? std::filesystem::path("ygdk_auto.png") : std::filesystem::path(photo_path);
    std::string boundary = "----UBAANextYgdkBoundary7MA4YWxkTrZu0gW";
    std::string upload_body;
    append_multipart_field(upload_body, boundary, "uid", m_uid);
    append_multipart_field(upload_body, boundary, "token", m_token);
    append_multipart_file(upload_body, boundary, "file", path, *file_content);
    upload_body += "--" + boundary + "--\r\n";

    HttpRequest upload;
    upload.method = HttpMethod::Post;
    upload.url = resolve_for_mode("https://ygdk.buaa.edu.cn/api/Front/Upload/File/post", m_mode);
    upload.headers["Accept"] = "application/json, text/plain, */*";
    upload.headers["Content-Type"] = "multipart/form-data; boundary=" + boundary;
    upload.headers["X-Requested-With"] = "XMLHttpRequest";
    upload.body = std::move(upload_body);
    auto upload_response = m_http_client.send(upload);
    if (!upload_response) return make_error(ErrorCode::NetworkError, "上传阳光打卡图片失败: " + upload_response.error().message);
    auto upload_data = unwrap_response(upload_response->body);
    if (upload_data.contains("__session_expired")) return make_error(ErrorCode::SessionExpired, "阳光打卡会话已过期，请重新登录");
    if (upload_data.contains("__error")) return make_error(ErrorCode::NetworkError, upload_data["__error"].get<std::string>());
    auto image_name = json_string(upload_data, "file_name");
    if (image_name.empty()) return make_error(ErrorCode::ParseError, "阳光打卡图片上传响应缺少 file_name");

    auto data = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/clockin",
                          {},
                          {{"start_time", resolved_time->start_epoch},
                           {"end_time", resolved_time->end_epoch},
                           {"place_type", "1"},
                           {"place", resolved_place},
                           {"isopen", share_to_square ? "1" : "0"},
                           {"form_time_fmt", resolved_time->formatted},
                           {"images", "[\"" + image_name + "\"]"},
                           {"classify_id", classify.id},
                           {"item_id", selected_item->id},
                           {"item_name", selected_item->name}});
    if (!data) return make_error(data.error().code, data.error().message);

    Model::MutationResult result;
    result.accepted = true;
    result.message = "阳光打卡提交成功";
    result.summary = make_record(json_string(*data, "record_id"), "阳光打卡", "submitted", {
        {"classifyId", classify.id},
        {"itemId", selected_item->id},
        {"itemName", selected_item->name},
        {"place", resolved_place},
        {"startTime", resolved_time->start_epoch},
        {"endTime", resolved_time->end_epoch},
        {"formTime", resolved_time->formatted},
        {"image", image_name},
        {"raw", data->dump()},
    });
    return result;
}

} // namespace UBAANext
