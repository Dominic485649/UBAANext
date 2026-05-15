#include <UBAANext/Service/SigninService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/SigninParser.hpp>

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
    return response.status_code == 401 || response.status_code == 403 ||
           response.body.find("name=\"execution\"") != std::string::npos ||
           response.body.find("统一身份认证") != std::string::npos;
}

std::string today_yyyymmdd() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
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
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::string> SigninService::get_student_id() {
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
        return student_id;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析用户信息 JSON 失败: ") + e.what());
    }
}

Result<std::pair<std::string, std::string>> SigninService::login_iclass(const std::string &student_id) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(
        "https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=" + url_encode_form(student_id) + "&userLevel=1&verificationType=2&verificationUrl=",
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
        if (json.value("STATUS", -1) != 0) {
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

Result<std::vector<Model::SigninCourse>> SigninService::list_today_courses() {
    (void)m_cache;

    auto student_id = get_student_id();
    if (!student_id) {
        return make_error(student_id.error().code, student_id.error().message);
    }

    auto session = login_iclass(*student_id);
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
    if (course_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "signin do 需要 --course-id <id>");
    }

    auto student_id = get_student_id();
    if (!student_id) {
        return make_error(student_id.error().code, student_id.error().message);
    }

    auto session = login_iclass(*student_id);
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
        bool success = json.value("STATUS", -1) == 0 && json_string(result, "stuSignStatus") == "1";
        auto raw_message = json_string(json, "ERRMSG");
        auto message = sanitize_signin_message(success, raw_message);
        if (!success) {
            return make_error(ErrorCode::NetworkError, message);
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
