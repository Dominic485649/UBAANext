#include <UBAANext/Service/FeatureService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>

#include <nlohmann/json.hpp>

#include <utility>

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

std::string parse_score_xq(const std::string &term_code) {
    auto last_dash = term_code.rfind('-');
    if (last_dash == std::string::npos) {
        return "";
    }
    auto term = term_code.substr(last_dash + 1);
    return term == "1" ? "1" : term == "2" ? "2" : "3";
}

std::string parse_score_year(const std::string &term_code) {
    auto first_dash = term_code.find('-');
    if (first_dash == std::string::npos) {
        return term_code;
    }
    return term_code.substr(0, first_dash);
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

std::vector<Model::FeatureRecord> records_for(const std::string &domain, const std::string &operation) {
    if (domain == "announcement") {
        return {make_record("ann-1", "系统公告", "published", {{"source", "mock"}, {"operation", operation}})};
    }
    if (domain == "grade") {
        return {make_record("grade-1", "高等数学", "posted", {{"score", "95"}, {"term", "2025-2026-2"}})};
    }
    if (domain == "spoc") {
        return {make_record("spoc-1", "SPOC 作业", "open", {{"deadline", "2026-06-01"}})};
    }
    if (domain == "judge") {
        return {make_record("judge-1", "评测任务", "open", {{"courseId", "course-1"}})};
    }
    if (domain == "signin") {
        return {make_record("signin-today", "今日签到", "available", {{"date", "today"}})};
    }
    if (domain == "evaluation") {
        return {make_record("evaluation-1", "课程评教", "pending", {{"course", "示例课程"}})};
    }
    if (domain == "ygdk") {
        return {make_record("ygdk-1", operation == "overview" ? "打卡概览" : "打卡记录", "ok", {{"operation", operation}})};
    }
    if (domain == "bykc") {
        return {make_record("bykc-1", operation == "chosen" ? "已选课程" : "选课课程", "available", {{"capacity", "30"}})};
    }
    if (domain == "cgyy") {
        return {make_record("cgyy-1", operation == "sites" ? "场馆" : "预约信息", "available", {{"operation", operation}})};
    }
    if (domain == "libbook") {
        return {make_record("libbook-1", operation == "libraries" ? "图书馆" : "座位预约", "available", {{"operation", operation}})};
    }
    return {make_record(domain + "-1", operation, "available", {{"domain", domain}, {"operation", operation}})};
}

} // namespace

FeatureService::FeatureService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_mode(mode) {
    (void)cache;
}

Result<Model::FeatureRecord> FeatureService::user_info() {
    if (m_mode == ConnectionMode::Mock) {
        return make_record("user", "模拟用户", "active", {{"studentId", "mock-user"}, {"college", "BUAA"}});
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
        auto name = data.value("name", data.value("realName", student_id));
        if (student_id.empty() && name.empty()) {
            return make_error(ErrorCode::SessionExpired, "用户信息会话已过期: 缺少用户字段");
        }
        return make_record(student_id.empty() ? "user" : student_id,
                           name.empty() ? "用户信息" : name,
                           "active",
                           {{"studentId", student_id}, {"rawCode", std::to_string(code)}});
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析用户信息 JSON 失败: ") + e.what());
    }
}

Result<std::vector<Model::FeatureRecord>> FeatureService::list(const std::string &domain, const std::string &operation) {
    if (m_mode == ConnectionMode::Mock) {
        return mock_list(domain, operation);
    }

    if (domain == "grade") {
        auto session = Protocol::Score::ensure_session(m_http_client, m_mode);
        if (!session) {
            return make_error(session.error().code, "激活成绩系统失败: " + session.error().message);
        }

        HttpRequest request;
        request.method = HttpMethod::Post;
        request.url = resolve_for_mode("https://app.buaa.edu.cn/buaascore/wap/default/index", m_mode);
        Protocol::Score::apply_form_headers(request);
        request.body = "xq=" + parse_score_xq(operation) + "&year=" + parse_score_year(operation);

        auto response = m_http_client.send(request);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "请求成绩失败: " + response.error().message);
        }
        if (Protocol::Score::is_session_expired_response(*response)) {
            return make_error(ErrorCode::SessionExpired, "成绩系统会话已过期");
        }
        if (response->status_code != 200) {
            return make_error(ErrorCode::NetworkError, "成绩请求返回: " + std::to_string(response->status_code));
        }

        try {
            auto json = nlohmann::json::parse(response->body);
            if (json.value("code", 0) != 0) {
                return make_error(ErrorCode::NetworkError, "成绩 API 返回错误");
            }
            std::vector<Model::FeatureRecord> grades;
            if (json.contains("data") && json["data"].is_object()) {
                for (const auto &[key, value] : json["data"].items()) {
                    std::map<std::string, std::string> fields;
                    if (value.is_object()) {
                        for (const auto &[field, field_value] : value.items()) {
                            if (field_value.is_string()) fields[field] = field_value.get<std::string>();
                            else if (field_value.is_number_integer()) fields[field] = std::to_string(field_value.get<int>());
                            else if (field_value.is_number_float()) fields[field] = std::to_string(field_value.get<double>());
                        }
                    }
                    fields["term"] = operation;
                    auto course_name_it = fields.find("kcmc");
                    grades.push_back(make_record(key,
                                                 course_name_it != fields.end() ? course_name_it->second : key,
                                                 "posted",
                                                 std::move(fields)));
                }
            }
            return grades;
        } catch (const std::exception &e) {
            return make_error(ErrorCode::ParseError, std::string("解析成绩 JSON 失败: ") + e.what());
        }
    }

    return make_error(ErrorCode::NotImplemented, domain + " " + operation + " 真实协议尚未接入，请先使用 --mock 验证命令形态");
}

Result<Model::FeatureRecord> FeatureService::show(const std::string &domain,
                                                  const std::string &operation,
                                                  const std::string &id) {
    if (id.empty()) {
        return make_error(ErrorCode::InvalidArgument, domain + " " + operation + " 需要 --id 或对应业务 ID");
    }
    if (m_mode != ConnectionMode::Mock) {
        return make_error(ErrorCode::NotImplemented, domain + " " + operation + " 真实协议尚未接入，请先使用 --mock 验证命令形态");
    }
    return mock_show(domain, operation, id);
}

Result<Model::MutationResult> FeatureService::mutate(const std::string &domain,
                                                     const std::string &operation,
                                                     const std::string &id,
                                                     bool confirmed) {
    if (!confirmed) {
        return make_error(ErrorCode::InvalidArgument, domain + " " + operation + " 是有副作用操作，必须显式传入 --confirm 或 --yes");
    }
    Model::MutationResult result;
    result.accepted = true;
    result.message = domain + " " + operation + " 已通过安全门";
    result.summary = make_record(id.empty() ? domain + "-mutation" : id, operation, "accepted", {{"domain", domain}});
    return result;
}

Result<std::vector<Model::FeatureRecord>> FeatureService::mock_list(const std::string &domain, const std::string &operation) const {
    return records_for(domain, operation);
}

Result<Model::FeatureRecord> FeatureService::mock_show(const std::string &domain,
                                                       const std::string &operation,
                                                       const std::string &id) const {
    auto records = records_for(domain, operation);
    auto record = records.empty() ? make_record(id, operation, "available") : records.front();
    record.id = id;
    return record;
}

} // namespace UBAANext
