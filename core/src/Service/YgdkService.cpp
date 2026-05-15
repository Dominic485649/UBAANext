#include <UBAANext/Service/YgdkService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/YgdkParser.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

namespace UBAANext {

namespace {

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
    if (item_id.empty()) return make_error(ErrorCode::InvalidArgument, "ygdk submit 需要 --item-id <id>");
    if (start_time.empty() || end_time.empty()) return make_error(ErrorCode::InvalidArgument, "ygdk submit 需要 --start-time 和 --end-time");
    if (place.empty()) return make_error(ErrorCode::InvalidArgument, "ygdk submit 需要 --place");

    auto login = ensure_session();
    if (!login) return make_error(login.error().code, login.error().message);

    std::string image_name;
    if (!photo_path.empty()) {
        auto file = read_binary_file(photo_path);
        if (!file) return make_error(file.error().code, file.error().message);
        auto path = std::filesystem::path(photo_path);
        std::string boundary = "----UBAANextYgdkBoundary7MA4YWxkTrZu0gW";
        std::string upload_body;
        append_multipart_field(upload_body, boundary, "uid", m_uid);
        append_multipart_field(upload_body, boundary, "token", m_token);
        append_multipart_file(upload_body, boundary, "file", path, *file);
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
        image_name = json_string(upload_data, "file_name");
        if (image_name.empty()) return make_error(ErrorCode::ParseError, "阳光打卡图片上传响应缺少 file_name");
    }

    auto classifies = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Classify/getList");
    if (!classifies) return make_error(classifies.error().code, classifies.error().message);
    auto list = classifies->contains("list") && (*classifies)["list"].is_array() ? (*classifies)["list"] : nlohmann::json::array();
    if (list.empty()) return make_error(ErrorCode::ParseError, "未获取到阳光打卡分类");
    auto classify = list.front();
    for (const auto &entry : list) {
        if (json_string(entry, "name").find("体育") != std::string::npos) {
            classify = entry;
            break;
        }
    }
    auto classify_id = json_string(classify, "classify_id");
    auto items = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Item/getList", {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify_id}}, {{"page", "1"}, {"limit", "1000"}, {"classify_id", classify_id}});
    if (!items) return make_error(items.error().code, items.error().message);
    std::string item_name;
    auto item_list = items->contains("list") && (*items)["list"].is_array() ? (*items)["list"] : nlohmann::json::array();
    for (const auto &entry : item_list) {
        if (json_string(entry, "item_id") == item_id) {
            item_name = json_string(entry, "name");
            break;
        }
    }
    if (item_name.empty()) return make_error(ErrorCode::InvalidArgument, "所选阳光打卡项目不存在: " + item_id);

    auto data = post_form("https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/clockin",
                          {},
                          {{"start_time", start_time},
                           {"end_time", end_time},
                           {"place_type", "1"},
                           {"place", place},
                           {"isopen", share_to_square ? "1" : "0"},
                           {"form_time_fmt", start_time + " - " + end_time},
                           {"images", image_name.empty() ? "[]" : "[\"" + image_name + "\"]"},
                           {"classify_id", classify_id},
                           {"item_id", item_id},
                           {"item_name", item_name}});
    if (!data) return make_error(data.error().code, data.error().message);

    Model::MutationResult result;
    result.accepted = true;
    result.message = "阳光打卡提交成功";
    result.summary = make_record(json_string(*data, "record_id"), "阳光打卡", "submitted", {{"raw", data->dump()}});
    return result;
}

} // namespace UBAANext
