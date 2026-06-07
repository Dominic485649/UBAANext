#include <UBAANext/Service/SrsService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Service/ResponseUtils.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <string_view>

namespace UBAANext {
namespace {

constexpr const char *kSrsHost = "byxk.buaa.edu.cn";
constexpr const char *kVpnHost = "d.buaa.edu.cn";
constexpr const char *kSrsTokenCookie = "token";
constexpr const char *kSrsLoginUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbyxk.buaa.edu.cn%2Fxsxk%2Fauth%2Fcas";
constexpr const char *kSrsConfigUrl = "https://byxk.buaa.edu.cn/xsxk/web/studentInfo";
constexpr const char *kSrsBatchUrl = "https://byxk.buaa.edu.cn/xsxk/profile/index.html";
constexpr const char *kSrsCourseListUrl = "https://byxk.buaa.edu.cn/xsxk/elective/buaa/clazz/list";
constexpr const char *kSrsPreselectedUrl = "https://byxk.buaa.edu.cn/xsxk/volunteer/select";
constexpr const char *kSrsSelectedUrl = "https://byxk.buaa.edu.cn/xsxk/elective/select";
constexpr const char *kSrsAddUrl = "https://byxk.buaa.edu.cn/xsxk/elective/buaa/clazz/add";
constexpr const char *kSrsDropUrl = "https://byxk.buaa.edu.cn/xsxk/elective/clazz/del";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    if (mode == ConnectionMode::WebVPN && url.rfind("https://d.buaa.edu.cn/", 0) != 0) return VpnCipher::to_vpn_url(url);
    return url;
}

std::string redacted_error_message(const std::string &message) {
    return Security::redact_sensitive_text(message);
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.is_object() || !json.contains(key) || json[key].is_null()) return {};
    const auto &value = json[key];
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<long long>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return {};
}

std::string first_string(const nlohmann::json &json, std::initializer_list<const char *> keys) {
    for (const auto *key : keys) {
        auto value = json_string(json, key);
        if (!value.empty()) return value;
    }
    return {};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string url_encode_component(const std::string &value) {
    std::ostringstream out;
    static constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << hex[(ch >> 4U) & 0x0fU] << hex[ch & 0x0fU];
        }
    }
    return out.str();
}

std::string append_query(const std::string &url, const std::string &key, const std::string &value) {
    return url + (url.find('?') == std::string::npos ? "?" : "&") + url_encode_component(key) + "=" + url_encode_component(value);
}

std::optional<std::string> token_from_cookie_store(ICookieStore *store, ConnectionMode mode) {
    if (!store) return std::nullopt;
    const auto read = [&](const CookieJar &jar) -> std::optional<std::string> {
        for (const auto *host : {kSrsHost, kVpnHost}) {
            auto token = jar.get_cookie(host, kSrsTokenCookie);
            if (token && !token->empty()) return token;
        }
        if (mode == ConnectionMode::WebVPN) {
            auto token = jar.get_cookie(kSrsTokenCookie);
            if (token && !token->empty()) return token;
        }
        return std::nullopt;
    };
    if (const auto *current = store->current()) {
        if (auto token = read(*current)) return token;
    }
    auto loaded = store->load();
    if (!loaded) return std::nullopt;
    return read(*loaded);
}

void apply_srs_headers(HttpRequest &request) {
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["User-Agent"] = kUserAgent;
    request.headers["Referer"] = "https://byxk.buaa.edu.cn/xsxk/profile/index.html";
}

Result<nlohmann::json> parse_srs_json(const HttpResponse &response, const std::string &context) {
    if (Protocol::is_session_expired_response(response, {}, true)) return make_error(ErrorCode::SessionExpired, context + "会话已过期，请重新登录");
    if (response.status_code < 200 || response.status_code >= 300) return make_error(ErrorCode::NetworkError, context + "请求返回: " + std::to_string(response.status_code));
    auto root = nlohmann::json::parse(response.body, nullptr, false);
    if (root.is_discarded()) return make_error(ErrorCode::ParseError, "解析" + context + "JSON 失败");
    if (root.is_object() && root.contains("code")) {
        const auto &code = root["code"];
        bool ok = false;
        if (code.is_number_integer()) ok = code.get<int>() == 200;
        else if (code.is_string()) {
            const auto value = lower_copy(code.get<std::string>());
            ok = value == "200" || value == "success" || value == "ok";
        }
        if (!ok) {
            auto message = first_string(root, {"msg", "message", "error"});
            if (message.empty()) message = context + "请求失败";
            return make_error(ErrorCode::NetworkError, Security::redact_sensitive_text(message));
        }
    }
    if (root.is_object() && root.contains("data")) return root["data"];
    return root;
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord course_record(const nlohmann::json &course, const std::string &scope, const std::string &status) {
    auto id = first_string(course, {"JXBID", "clazzId", "id"});
    auto title = first_string(course, {"KCM", "courseName", "name"});
    auto teacher = first_string(course, {"SKJSZC", "SKJS", "teacher"});
    auto selected = first_string(course, {"SFYX", "selected"});
    auto conflict = first_string(course, {"SFCT", "conflict"});
    auto can_drop = first_string(course, {"SFKT", "canDrop"});
    return make_record(id, title, status,
                       {{"scope", scope.empty() ? first_string(course, {"teachingClassType"}) : scope},
                        {"courseCode", first_string(course, {"KCH", "course_id"})},
                        {"courseIndex", first_string(course, {"KXH"})},
                        {"campus", first_string(course, {"XQ"})},
                        {"teacher", teacher},
                        {"department", first_string(course, {"KKDW"})},
                        {"credit", first_string(course, {"XF"})},
                        {"classHours", first_string(course, {"XS"})},
                        {"selected", selected},
                        {"conflict", conflict},
                        {"canDrop", can_drop},
                        {"secretVal", first_string(course, {"secretVal"})}});
}

void append_selected_records(std::vector<Model::FeatureRecord> &records, const nlohmann::json &value, const std::string &group) {
    if (!value.is_array()) return;
    for (const auto &item : value) {
        auto record = course_record(item, first_string(item, {"teachingClassType"}), "selected");
        if (!group.empty()) record.fields["volunteerGroup"] = group;
        records.push_back(std::move(record));
    }
}

std::string form_body(const Model::SrsCourseOperation &operation, bool preselect) {
    std::vector<std::pair<std::string, std::string>> pairs{
        {"clazzType", operation.scope},
        {"clazzId", operation.class_id},
        {"secretVal", operation.secret},
    };
    if (preselect && !operation.batch_id.empty()) pairs.push_back({"batchId", operation.batch_id});
    if (preselect && operation.volunteer_index > 0) pairs.push_back({"chooseVolunteer", std::to_string(operation.volunteer_index)});
    std::ostringstream out;
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (i > 0) out << '&';
        out << url_encode_component(pairs[i].first) << '=' << url_encode_component(pairs[i].second);
    }
    return out.str();
}

Result<void> validate_operation(const Model::SrsCourseOperation &operation, bool preselect) {
    if (operation.scope.empty()) return make_error(ErrorCode::InvalidArgument, "srs 写操作需要 --scope <scope>");
    if (operation.class_id.empty()) return make_error(ErrorCode::InvalidArgument, "srs 写操作需要 --id <clazzId>");
    if (operation.secret.empty()) return make_error(ErrorCode::InvalidArgument, "srs 写操作需要 --token <secretVal>");
    if (preselect && operation.batch_id.empty()) return make_error(ErrorCode::InvalidArgument, "srs course preselect 需要 --batch-id <batchId>");
    if (preselect && operation.volunteer_index <= 0) return make_error(ErrorCode::InvalidArgument, "srs course preselect 需要 --index <n>");
    return {};
}

Model::MutationResult mutation_result(std::string id, std::string message, std::string status) {
    Model::MutationResult result;
    result.accepted = true;
    result.message = std::move(message);
    result.summary = make_record(std::move(id), result.message, std::move(status));
    return result;
}

} // namespace

SrsService::SrsService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cookie_store(cookie_store), m_cache(cache), m_mode(mode) {}

Result<std::string> SrsService::token(bool force_refresh) {
    (void)m_cache;
    if (!force_refresh && m_token && !m_token->empty()) return *m_token;
    if (!m_cookie_store) return make_error(ErrorCode::UnsupportedCookiePersistence, "SRS 需要 CookieStore 读取 byxk token");
    if (!force_refresh) {
        if (auto cached = token_from_cookie_store(m_cookie_store, m_mode)) {
            m_token = *cached;
            return *m_token;
        }
    }

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(kSrsLoginUrl, m_mode);
    apply_srs_headers(request);
    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, "SRS 登录激活失败: " + redacted_error_message(response.error().message));
    if (response->status_code < 200 || response->status_code >= 400) return make_error(ErrorCode::NetworkError, "SRS 登录激活返回: " + std::to_string(response->status_code));
    if (auto activated = token_from_cookie_store(m_cookie_store, m_mode)) {
        m_token = *activated;
        return *m_token;
    }
    return make_error(ErrorCode::SessionExpired, "SRS 未获得 byxk token，请重新登录");
}

Result<HttpResponse> SrsService::send_srs_request(HttpRequest request, const std::string &context, bool token_query) {
    auto ready = token();
    if (!ready) return make_error(ready.error().code, ready.error().message);
    request.headers["Authorization"] = *ready;
    if (token_query) request.url = append_query(request.url, "token", *ready);
    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, context + "请求失败: " + redacted_error_message(response.error().message));
    if (response->status_code == 401 || response->status_code == 403 || Protocol::is_session_expired_response(*response, {}, true)) {
        m_token.reset();
        auto refreshed = token(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        request.headers["Authorization"] = *refreshed;
        response = m_http_client.send(request);
        if (!response) return make_error(response.error().code, context + "请求失败: " + redacted_error_message(response.error().message));
    }
    return *response;
}

Result<std::vector<Model::FeatureRecord>> SrsService::config() {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsConfigUrl, m_mode);
    apply_srs_headers(request);
    auto response = send_srs_request(std::move(request), "SRS 配置", true);
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 配置");
    if (!json) return make_error(json.error().code, json.error().message);
    const auto &student = json->contains("student") ? (*json)["student"] : *json;
    std::vector<Model::FeatureRecord> records;
    records.push_back(make_record("config", "SRS config", "ok", {{"campus", first_string(student, {"campus"})}}));
    if (student.is_object() && student.contains("electiveBatchList") && student["electiveBatchList"].is_array()) {
        for (const auto &batch : student["electiveBatchList"]) {
            records.push_back(make_record(first_string(batch, {"code", "id"}), first_string(batch, {"name"}), "batch",
                                          {{"canSelect", first_string(batch, {"canSelect"})},
                                           {"beginTime", first_string(batch, {"beginTime"})},
                                           {"endTime", first_string(batch, {"endTime"})}}));
        }
    }
    return records;
}

Result<Model::FeatureRecord> SrsService::batch() {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(kSrsBatchUrl, m_mode);
    apply_srs_headers(request);
    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, "SRS 批次请求失败: " + redacted_error_message(response.error().message));
    if (response->status_code < 200 || response->status_code >= 300) return make_error(ErrorCode::NetworkError, "SRS 批次请求返回: " + std::to_string(response->status_code));
    std::smatch match;
    if (!std::regex_search(response->body, match, std::regex{"\"code\"\\s*:\\s*\"([^\"]+)\""})) {
        return make_error(ErrorCode::ParseError, "SRS 批次页面缺少 code");
    }
    return make_record(match[1].str(), "SRS batch", "batch");
}

Result<std::vector<Model::FeatureRecord>> SrsService::courses(const Model::SrsCourseFilter &filter) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsCourseListUrl, m_mode);
    apply_srs_headers(request);
    request.headers["Content-Type"] = "application/json";
    nlohmann::json body{{"teachingClassType", filter.scope}, {"pageNumber", filter.page}, {"pageSize", filter.size}, {"campus", filter.campus}};
    if (!filter.display_conflict) body["SFCT"] = "0";
    if (!filter.requirement.empty()) body["KCXZ"] = filter.requirement;
    if (!filter.category.empty()) body["KCLB"] = filter.category;
    if (!filter.keyword.empty()) body["KEY"] = filter.keyword;
    request.body = body.dump();
    auto response = send_srs_request(std::move(request), "SRS 课程查询");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 课程查询");
    if (!json) return make_error(json.error().code, json.error().message);
    const auto &rows = json->is_object() && json->contains("rows") ? (*json)["rows"] : *json;
    std::vector<Model::FeatureRecord> records;
    if (rows.is_array()) {
        for (const auto &course : rows) records.push_back(course_record(course, filter.scope, "course"));
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> SrsService::preselected() {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsPreselectedUrl, m_mode);
    apply_srs_headers(request);
    auto response = send_srs_request(std::move(request), "SRS 预选查询");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 预选查询");
    if (!json) return make_error(json.error().code, json.error().message);
    std::vector<Model::FeatureRecord> records;
    if (json->is_array()) {
        int group = 1;
        for (const auto &value : *json) {
            if (value.is_object() && value.contains("tcList")) append_selected_records(records, value["tcList"], std::to_string(group));
            else append_selected_records(records, value, std::to_string(group));
            ++group;
        }
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> SrsService::selected() {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsSelectedUrl, m_mode);
    apply_srs_headers(request);
    auto response = send_srs_request(std::move(request), "SRS 已选查询");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 已选查询");
    if (!json) return make_error(json.error().code, json.error().message);
    std::vector<Model::FeatureRecord> records;
    append_selected_records(records, *json, {});
    return records;
}

void SrsService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> SrsService::preselect_course(const Model::SrsCourseOperation &operation) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto valid = validate_operation(operation, true);
    if (!valid) return make_error(valid.error().code, valid.error().message);
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsAddUrl, m_mode);
    apply_srs_headers(request);
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.body = form_body(operation, true);
    auto response = send_srs_request(std::move(request), "SRS 预选课程");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 预选课程");
    if (!json) return make_error(json.error().code, json.error().message);
    return mutation_result(operation.class_id, "SRS course preselected", "preselected");
}

Result<Model::MutationResult> SrsService::select_course(const Model::SrsCourseOperation &operation) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto valid = validate_operation(operation, false);
    if (!valid) return make_error(valid.error().code, valid.error().message);
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsAddUrl, m_mode);
    apply_srs_headers(request);
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.body = form_body(operation, false);
    auto response = send_srs_request(std::move(request), "SRS 正选课程");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 正选课程");
    if (!json) return make_error(json.error().code, json.error().message);
    return mutation_result(operation.class_id, "SRS course selected", "selected");
}

Result<Model::MutationResult> SrsService::drop_course(const Model::SrsCourseOperation &operation) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto valid = validate_operation(operation, false);
    if (!valid) return make_error(valid.error().code, valid.error().message);
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(kSrsDropUrl, m_mode);
    apply_srs_headers(request);
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.body = form_body(operation, false);
    auto response = send_srs_request(std::move(request), "SRS 退选课程");
    if (!response) return make_error(response.error().code, response.error().message);
    auto json = parse_srs_json(*response, "SRS 退选课程");
    if (!json) return make_error(json.error().code, json.error().message);
    return mutation_result(operation.class_id, "SRS course dropped", "dropped");
}

} // namespace UBAANext
