#include <UBAANext/Service/SigninService.hpp>

#include <UBAANext/Base/TimeUtils.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/SigninParser.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace UBAANext {

namespace {

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

bool response_is_sso(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response, {}, true);
}

std::string today_yyyymmdd() {
    std::time_t now = std::time(nullptr);
    auto local = local_time(now);
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d");
    return out.str();
}

std::string url_encode_form(const std::string &value) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

std::string url_decode_component(const std::string &value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            char hex[] = {value[i + 1], value[i + 2], '\0'};
            char *end = nullptr;
            auto decoded = std::strtol(hex, &end, 16);
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

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string extract_signin_login_name(const std::string &url) {
    auto normalized = VpnCipher::from_vpn_url(url);
    auto pos = normalized.find_first_of("?#");
    while (pos != std::string::npos && pos + 1 < normalized.size()) {
        std::size_t cursor = pos + 1;
        while (cursor <= normalized.size()) {
            auto next = normalized.find_first_of("&#", cursor);
            auto pair = normalized.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor);
            auto eq = pair.find('=');
            if (eq != std::string::npos && lower_copy(pair.substr(0, eq)) == "loginname") {
                return url_decode_component(pair.substr(eq + 1));
            }
            if (next == std::string::npos) break;
            if (normalized[next] == '#') {
                pos = next;
                break;
            }
            cursor = next + 1;
        }
        auto next_hash = normalized.find('#', pos + 1);
        if (next_hash == std::string::npos || next_hash == pos) break;
        pos = next_hash;
    }
    return {};
}

std::string signin_header_value(const HttpResponse &response, const std::string &name) {
    auto value = Protocol::header_value(response, name);
    auto newline = value.find('\n');
    return newline == std::string::npos ? value : value.substr(0, newline);
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

Model::FeatureRecord course_to_record(const Model::SigninCourse &course) {
    return make_record(course.id, course.name, course.status, {
        {"classBeginTime", course.class_begin_time},
        {"classEndTime", course.class_end_time},
        {"signStatus", course.sign_status},
    });
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

bool json_status_success(const nlohmann::json &json) {
    auto status = json_string(json, "STATUS");
    if (status.empty()) status = json_string(json, "code");
    return status == "0" || status == "200" || status == "success";
}

std::string sanitize_signin_message(bool success, const std::string &raw_message) {
    if (success) {
        return raw_message.empty() ? "签到成功" : raw_message;
    }
    if (raw_message.find("已签到") != std::string::npos) {
        return "您今天已经签到过了";
    }
    if (raw_message.find("未开始") != std::string::npos) {
        return "当前还未到签到时间";
    }
    if (raw_message.find("不是上课时间") != std::string::npos) {
        return "当前不是上课时间，无法签到";
    }
    if (raw_message.find("已结束") != std::string::npos) {
        return "本次签到已结束";
    }
    if (raw_message.find("范围") != std::string::npos) {
        return "当前不在可签到范围内";
    }
    if (raw_message.find("课程") != std::string::npos && raw_message.find("不存在") != std::string::npos) {
        return "未找到对应课程，请刷新后重试";
    }
    return "签到失败，请稍后重试";
}

} // namespace

SigninService::SigninService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : SigninService(http_client, cache, mode, {}) {}

SigninService::SigninService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, std::string student_id)
    : m_http_client(http_client), m_cache(cache), m_mode(mode), m_student_id(std::move(student_id)) {}

Result<std::string> SigninService::get_student_id() {
    if (!m_student_id.empty()) {
        return m_student_id;
    }

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://uc.buaa.edu.cn/api/uc/userinfo", m_mode);
    request.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["User-Agent"] = "UBAANext/0.4";

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求用户信息失败: " + response.error().message);
    }
    if (response_is_sso(*response)) {
        return make_error(ErrorCode::SessionExpired, "用户信息会话已过期");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "用户信息请求返回: " + std::to_string(response->status_code));
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        int code = json.value("code", 0);
        if (code != 0) {
            return make_error(ErrorCode::SessionExpired, "用户信息会话已过期: code=" + std::to_string(code));
        }
        auto data = json.contains("data") && json["data"].is_object() ? json["data"] : json;
        auto student_id = data.value("schoolid", data.value("username", ""));
        if (student_id.empty()) {
            return make_error(ErrorCode::SessionExpired, "用户信息缺少学号字段");
        }
        m_student_id = student_id;
        return student_id;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析用户信息 JSON 失败: ") + e.what());
    }
}

Result<std::string> SigninService::resolve_login_name() {
    const std::string initial_url = "https://iclass.buaa.edu.cn:8346/?type=jumpMyCenter";
    if (auto login_name = extract_signin_login_name(initial_url); !login_name.empty()) {
        return login_name;
    }

    std::string current_url = initial_url;
    for (int redirect = 0; redirect <= 8; ++redirect) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, m_mode);
        request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        request.headers["User-Agent"] = "UBAANext/0.4";
        Protocol::disable_transport_redirects(request);

        auto response = m_http_client.send(request);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "解析签到登录名失败: " + response.error().message);
        }
        if (auto login_name = extract_signin_login_name(current_url); !login_name.empty()) {
            return login_name;
        }
        auto location = signin_header_value(*response, "Location");
        if (auto login_name = extract_signin_login_name(location); !login_name.empty()) {
            return login_name;
        }
        if (response->status_code < 300 || response->status_code >= 400 || location.empty()) {
            break;
        }
        current_url = Protocol::resolve_location(current_url, location);
        if (auto login_name = extract_signin_login_name(current_url); !login_name.empty()) {
            return login_name;
        }
    }
    return make_error(ErrorCode::SessionExpired, "签到系统登录名解析失败");
}

Result<std::pair<std::string, std::string>> SigninService::login_iclass(const std::string &login_name) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(
        "https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=" + url_encode_form(login_name) + "&userLevel=1&verificationType=2&verificationUrl=",
        m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["User-Agent"] = "UBAANext/0.4";

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "登录签到系统失败: " + response.error().message);
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "签到系统登录返回: " + std::to_string(response->status_code));
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        if (!json_status_success(json)) {
            return make_error(ErrorCode::SessionExpired, "签到系统登录失败");
        }
        auto result = json.contains("result") && json["result"].is_object() ? json["result"] : nlohmann::json::object();
        auto user_id = json_string(result, "id");
        auto session_id = json_string(result, "sessionId");
        if (user_id.empty() || session_id.empty()) {
            return make_error(ErrorCode::ParseError, "签到系统登录响应缺少 id 或 sessionId");
        }
        return std::make_pair(user_id, session_id);
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析签到系统登录 JSON 失败: ") + e.what());
    }
}

Result<std::pair<std::string, std::string>> SigninService::ensure_iclass_session(bool force_refresh) {
    if (!force_refresh && !m_user_id.empty() && !m_session_id.empty()) {
        return std::make_pair(m_user_id, m_session_id);
    }
    if (force_refresh) {
        m_user_id.clear();
        m_session_id.clear();
    }
    auto student_id = get_student_id();
    if (!student_id) return make_error(student_id.error().code, student_id.error().message);
    auto login_name = resolve_login_name();
    if (!login_name) login_name = *student_id;
    auto session = login_iclass(*login_name);
    if (!session) return make_error(session.error().code, session.error().message);
    m_user_id = session->first;
    m_session_id = session->second;
    return *session;
}

Result<std::vector<Model::SigninCourse>> SigninService::list_today_courses() {
    return list_today_courses(true);
}

Result<std::vector<Model::SigninCourse>> SigninService::list_today_courses(bool allow_retry) {
    (void)m_cache;

    auto session = ensure_iclass_session();
    if (!session) {
        return make_error(session.error().code, session.error().message);
    }

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(
        "https://iclass.buaa.edu.cn:8347/app/course/get_stu_course_sched.action?id=" + url_encode_form(session->first) + "&dateStr=" + today_yyyymmdd(),
        m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["sessionId"] = session->second;
    request.headers["User-Agent"] = "UBAANext/0.4";

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求今日签到失败: " + response.error().message);
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "今日签到请求返回: " + std::to_string(response->status_code));
    }

    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (json.is_discarded()) {
        return make_error(ErrorCode::ParseError, "解析今日签到 JSON 失败");
    }
    if (json.contains("STATUS") && !json_status_success(json)) {
        if (!allow_retry) return Parser::parse_signin_today_courses(json);
        auto refreshed = ensure_iclass_session(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        return list_today_courses(false);
    }
    return Parser::parse_signin_today_courses(json);
}

Result<std::vector<Model::FeatureRecord>> SigninService::list_today() {
    auto result = list_today_courses();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &course : *result) records.push_back(course_to_record(course));
    return records;
}

Result<Model::MutationResult> SigninService::perform_signin(const std::string &course_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    return perform_signin_once(course_id, true);
}

void SigninService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> SigninService::perform_signin_once(const std::string &course_id, bool allow_retry) {
    if (course_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "signin do 需要 --course-id <id>");
    }

    auto session = ensure_iclass_session();
    if (!session) {
        return make_error(session.error().code, session.error().message);
    }

    HttpRequest timestamp_request;
    timestamp_request.method = HttpMethod::Get;
    timestamp_request.url = resolve_for_mode("http://iclass.buaa.edu.cn:8081/app/common/get_timestamp.action", m_mode);
    timestamp_request.headers["Accept"] = "application/json, text/plain, */*";
    timestamp_request.headers["User-Agent"] = "UBAANext/0.4";

    auto timestamp_response = m_http_client.send(timestamp_request);
    if (!timestamp_response) {
        return make_error(ErrorCode::NetworkError, "获取签到服务器时间失败: " + timestamp_response.error().message);
    }
    if (timestamp_response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "签到服务器时间请求返回: " + std::to_string(timestamp_response->status_code));
    }

    std::string timestamp;
    try {
        auto json = nlohmann::json::parse(timestamp_response->body);
        timestamp = json_string(json, "timestamp");
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析签到服务器时间 JSON 失败: ") + e.what());
    }
    if (timestamp.empty()) {
        return make_error(ErrorCode::ParseError, "签到服务器时间响应缺少 timestamp");
    }

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(
        "http://iclass.buaa.edu.cn:8081/app/course/stu_scan_sign.action?courseSchedId=" + url_encode_form(course_id) + "&timestamp=" + url_encode_form(timestamp),
        m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.headers["sessionId"] = session->second;
    request.headers["User-Agent"] = "UBAANext/0.4";
    request.body = "id=" + url_encode_form(session->first);

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "提交签到失败: " + response.error().message);
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "签到提交返回: " + std::to_string(response->status_code));
    }

    try {
        auto json = nlohmann::json::parse(response->body);
        auto result = json.contains("result") && json["result"].is_object() ? json["result"] : nlohmann::json::object();
        auto sign_status = json_string(result, "stuSignStatus");
        bool success = json_status_success(json) && (sign_status == "1" || sign_status == "success");
        auto raw_message = json_string(json, "ERRMSG");
        auto message = sanitize_signin_message(success, raw_message);
        if (!success) {
            if (allow_retry && raw_message.find("登录") != std::string::npos) {
                auto refreshed = ensure_iclass_session(true);
                if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
                return perform_signin_once(course_id, false);
            }
            return make_error(ErrorCode::InvalidArgument, message);
        }

        Model::MutationResult mutation;
        mutation.accepted = true;
        mutation.message = message;
        mutation.summary = make_record(course_id, "签到", "signed", {{"courseId", course_id}});
        return mutation;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析签到提交 JSON 失败: ") + e.what());
    }
}

} // namespace UBAANext
