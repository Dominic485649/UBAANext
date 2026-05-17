#include <UBAANext/Service/FeatureService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Version.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>
#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Service/YgdkService.hpp>

#include <nlohmann/json.hpp>

#include <charconv>
#include <sstream>
#include <utility>
#include <vector>

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

std::vector<std::string> split_colon_fields(const std::string &text) {
    std::vector<std::string> parts;
    std::string item;
    std::istringstream input(text);
    while (std::getline(input, item, ':')) {
        parts.push_back(item);
    }
    return parts;
}

Result<int> parse_positive_int(const std::string &value, const std::string &name) {
    int parsed = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed < 1) {
        return make_error(ErrorCode::InvalidArgument, name + " 必须是正整数");
    }
    return parsed;
}

Result<int> parse_non_negative_int(const std::string &value, const std::string &name) {
    int parsed = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed < 0) {
        return make_error(ErrorCode::InvalidArgument, name + " 必须是非负整数");
    }
    return parsed;
}

Result<std::pair<int, int>> parse_page_size_operation(const std::string &text, const std::string &name, bool zero_based_page = false) {
    auto sep = text.find(':');
    auto page_text = sep == std::string::npos ? text : text.substr(0, sep);
    auto page = zero_based_page ? parse_non_negative_int(page_text, name + " page") : parse_positive_int(page_text, name + " page");
    if (!page) return make_error(page.error().code, page.error().message);
    auto size = sep == std::string::npos ? Result<int>(20) : parse_positive_int(text.substr(sep + 1), name + " size");
    if (!size) return make_error(size.error().code, size.error().message);
    return std::make_pair(*page, *size);
}

#if UBAANEXT_ENABLE_MOCKS
std::vector<Model::FeatureRecord> records_for(const std::string &domain, const std::string &operation) {
    if (domain == "announcement") {
        return {make_record("ann-1", "系统公告", "published", {{"source", "mock"}, {"operation", operation}})};
    }
    if (domain == "app") {
        return {make_record("app-version", "UBAA Next", "current", {{"version", UBAANEXT_VERSION_STRING}, {"source", "mock"}, {"operation", operation}})};
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
    return {};
}
#endif

} // namespace

FeatureService::FeatureService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<Model::FeatureRecord> FeatureService::user_info() {
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        return make_record("user", "模拟用户", "active", {{"studentId", "mock-user"}, {"college", "BUAA"}});
    }
#endif

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
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        return mock_list(domain, operation);
    }
#endif

    if (domain == "app") {
        if (operation == "version") {
            return std::vector<Model::FeatureRecord>{make_record("app-version", "UBAA Next", "current", {{"version", UBAANEXT_VERSION_STRING}, {"api", "local-cli"}})};
        }
        return make_error(ErrorCode::InvalidArgument, "未知的 app 查询操作: " + operation);
    }

    if (domain == "announcement") {
        if (operation == "list") {
            return make_error(ErrorCode::NotImplemented, "公告真实协议尚未接入");
        }
        return make_error(ErrorCode::InvalidArgument, "未知的公告查询操作: " + operation);
    }

    if (domain == "judge") {
        JudgeService service(m_http_client, m_cache, m_mode);
        JudgeAssignmentQuery query;
        if (operation.rfind("assignments", 0) == 0) {
            if (operation.rfind("assignments:", 0) == 0) {
                auto parts = split_colon_fields(operation.substr(12));
                if (!parts.empty()) query.course_id = parts[0];
                query.include_expired = parts.size() > 1 && parts[1] == "include-expired";
                query.include_history = parts.size() > 2 && parts[2] == "include-history";
            }
        } else {
            query.course_id = operation;
        }
        return service.list_assignments(query);
    }

    if (domain == "spoc" && operation.rfind("assignments", 0) == 0) {
        SpocService service(m_http_client, m_cache, m_mode);
        SpocAssignmentQuery query;
        if (operation.rfind("assignments:", 0) == 0) {
            auto parts = split_colon_fields(operation.substr(12));
            query.pending_only = !parts.empty() && parts[0] == "pending";
            query.include_expired = parts.size() > 1 && parts[1] == "include-expired";
        }
        return service.list_assignments(query);
    }

    if (domain == "signin" && operation == "today") {
        SigninService service(m_http_client, m_cache, m_mode);
        return service.list_today();
    }

    if (domain == "evaluation" && operation == "list") {
        EvaluationService service(m_http_client, m_cache, m_mode);
        return service.list_evaluations();
    }

    if (domain == "ygdk") {
        YgdkService service(m_http_client, m_cache, m_mode);
        if (operation == "overview") {
            return service.overview();
        }
        if (operation == "records") {
            return service.records();
        }
        if (operation.rfind("records:", 0) == 0) {
            auto pagination = parse_page_size_operation(operation.substr(8), "ygdk records");
            if (!pagination) return make_error(pagination.error().code, pagination.error().message);
            return service.records(pagination->first, pagination->second);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的 ygdk 查询操作: " + operation);
    }

    if (domain == "bykc") {
        BykcService service(m_http_client, m_cache, m_mode);
        if (operation == "profile") return service.profile();
        if (operation == "courses") return service.courses();
        if (operation.rfind("courses:", 0) == 0) {
            auto parts = split_colon_fields(operation.substr(8));
            BykcCourseQuery query;
            if (!parts.empty() && !parts[0].empty()) {
                auto page = parse_positive_int(parts[0], "bykc courses page");
                if (!page) return make_error(page.error().code, page.error().message);
                query.page = *page;
            }
            if (parts.size() > 1 && !parts[1].empty()) {
                auto size = parse_positive_int(parts[1], "bykc courses size");
                if (!size) return make_error(size.error().code, size.error().message);
                query.size = *size;
            }
            if (parts.size() > 2) query.all = parts[2] == "all";
            if (parts.size() > 3) query.status = parts[3];
            if (parts.size() > 4) query.category = parts[4];
            if (parts.size() > 5) query.sub_category = parts[5];
            if (parts.size() > 6) query.campus = parts[6];
            if (parts.size() > 7) query.keyword = parts[7];
            return service.courses(query);
        }
        if (operation == "chosen") return service.chosen();
        if (operation == "stats") return service.stats();
        return make_error(ErrorCode::InvalidArgument, "未知的 bykc 查询操作: " + operation);
    }

    if (domain == "cgyy") {
        VenueReservationService service(m_http_client, m_cache, m_mode);
        if (operation == "sites") {
            return service.list_sites();
        }
        if (operation == "purpose-types") {
            return service.list_purpose_types();
        }
        if (operation == "day-info") {
            return make_error(ErrorCode::InvalidArgument, "cgyy day-info 需要 --date 和 --id/--site-id");
        }
        if (operation.rfind("day-info:", 0) == 0) {
            auto rest = operation.substr(9);
            auto sep = rest.find(':');
            return service.day_info(sep == std::string::npos ? "" : rest.substr(0, sep), sep == std::string::npos ? rest : rest.substr(sep + 1));
        }
        if (operation == "orders") {
            return service.list_orders();
        }
        if (operation.rfind("orders:", 0) == 0) {
            auto pagination = parse_page_size_operation(operation.substr(7), "cgyy orders", true);
            if (!pagination) return make_error(pagination.error().code, pagination.error().message);
            return service.list_orders(pagination->first, pagination->second);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的 cgyy 查询操作: " + operation);
    }

    if (domain == "libbook") {
        LibrarySeatService service(m_http_client, m_cache, m_mode);
        if (operation == "libraries") {
            return service.list_libraries("");
        }
        if (operation == "reservations") {
            return service.list_reservations();
        }
        if (operation.rfind("reservations:", 0) == 0) {
            auto pagination = parse_page_size_operation(operation.substr(13), "libbook reservations");
            if (!pagination) return make_error(pagination.error().code, pagination.error().message);
            return service.list_reservations(pagination->first, pagination->second);
        }
        if (operation == "areas") {
            return service.list_areas("", "");
        }
        if (operation == "seats") {
            return service.list_seats("", "");
        }
        if (operation.rfind("areas:", 0) == 0) {
            auto rest = operation.substr(6);
            auto first = rest.find(':');
            auto second = first == std::string::npos ? std::string::npos : rest.find(':', first + 1);
            return service.list_areas(first == std::string::npos ? rest : rest.substr(0, first),
                                      first == std::string::npos ? "" : second == std::string::npos ? rest.substr(first + 1) : rest.substr(first + 1, second - first - 1),
                                      second == std::string::npos ? "" : rest.substr(second + 1));
        }
        if (operation.rfind("seats:", 0) == 0) {
            auto rest = operation.substr(6);
            auto first = rest.find(':');
            auto second = first == std::string::npos ? std::string::npos : rest.find(':', first + 1);
            auto third = second == std::string::npos ? std::string::npos : rest.find(':', second + 1);
            return service.list_seats(first == std::string::npos ? rest : rest.substr(0, first),
                                      first == std::string::npos ? "" : second == std::string::npos ? rest.substr(first + 1) : rest.substr(first + 1, second - first - 1),
                                      second == std::string::npos ? "" : third == std::string::npos ? rest.substr(second + 1) : rest.substr(second + 1, third - second - 1),
                                      third == std::string::npos ? "" : rest.substr(third + 1));
        }
        return make_error(ErrorCode::InvalidArgument, "未知的 libbook 查询操作: " + operation);
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

    return make_error(ErrorCode::NotImplemented, domain + " " + operation + " 真实协议尚未接入");
}

Result<Model::FeatureRecord> FeatureService::show(const std::string &domain,
                                                  const std::string &operation,
                                                  const std::string &id) {
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        return mock_show(domain, operation, id.empty() ? operation : id);
    }
#endif
    if (domain == "cgyy" && operation == "lock-code") {
        VenueReservationService service(m_http_client, m_cache, m_mode);
        return service.lock_code();
    }
    if (id.empty()) {
        return make_error(ErrorCode::InvalidArgument, domain + " " + operation + " 需要 --id 或对应业务 ID");
    }
    if (domain == "judge") {
        JudgeService service(m_http_client, m_cache, m_mode);
        if (operation == "details") {
            return service.show_assignment_details(id);
        }
        return service.show_assignment(id);
    }
    if (domain == "spoc" && operation == "assignment") {
        SpocService service(m_http_client, m_cache, m_mode);
        return service.show_assignment(id);
    }
    if (domain == "bykc" && operation == "course") {
        BykcService service(m_http_client, m_cache, m_mode);
        return service.show_course(id);
    }
    if (domain == "cgyy") {
        VenueReservationService service(m_http_client, m_cache, m_mode);
        if (operation == "show") {
            return service.show_order(id);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的 cgyy 详情操作: " + operation);
    }
    if (domain == "libbook" && operation == "area") {
        LibrarySeatService service(m_http_client, m_cache, m_mode);
        return service.show_area(id);
    }
    return make_error(ErrorCode::NotImplemented, domain + " " + operation + " 真实协议尚未接入");
}

Result<Model::MutationResult> FeatureService::mutate(const std::string &domain,
                                                     const std::string &operation,
                                                     const std::string &id,
                                                     bool confirmed) {
    if (!confirmed) {
        return make_error(ErrorCode::InvalidArgument, domain + " " + operation + " 是有副作用操作，必须显式传入 --confirm 或 --yes");
    }

#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        Model::MutationResult result;
        result.accepted = true;
        result.message = domain + " " + operation + " 已通过安全门";
        result.summary = make_record(id.empty() ? domain + "-mutation" : id, operation, "accepted", {{"domain", domain}});
        return result;
    }
#endif

    if (domain == "signin" && operation == "do") {
        SigninService service(m_http_client, m_cache, m_mode);
        return service.perform_signin(id);
    }

    if (domain == "bykc" && operation == "select") {
        BykcService service(m_http_client, m_cache, m_mode);
        return service.select_course(id);
    }
    if (domain == "bykc" && operation == "unselect") {
        BykcService service(m_http_client, m_cache, m_mode);
        return service.unselect_course(id);
    }
    if (domain == "bykc" && operation.rfind("sign:", 0) == 0) {
        auto first = operation.find(':');
        if (first == std::string::npos) {
            return make_error(ErrorCode::InvalidArgument, "bykc sign 需要 --sign-type");
        }
        auto sign_type = parse_positive_int(operation.substr(first + 1), "bykc sign type");
        if (!sign_type) return make_error(sign_type.error().code, sign_type.error().message);
        BykcService service(m_http_client, m_cache, m_mode);
        return service.sign_course(id, *sign_type);
    }
    if (domain == "evaluation" && operation == "submit") {
        EvaluationService service(m_http_client, m_cache, m_mode);
        return service.submit_evaluations(id);
    }
    if (domain == "ygdk" && operation.rfind("submit:", 0) == 0) {
        auto rest = operation.substr(7);
        auto first = rest.find('\n');
        auto second = first == std::string::npos ? std::string::npos : rest.find('\n', first + 1);
        auto third = second == std::string::npos ? std::string::npos : rest.find('\n', second + 1);
        auto fourth = third == std::string::npos ? std::string::npos : rest.find('\n', third + 1);
        if (first == std::string::npos || second == std::string::npos || third == std::string::npos || fourth == std::string::npos) {
            return make_error(ErrorCode::InvalidArgument, "ygdk submit 参数不完整");
        }
        YgdkService service(m_http_client, m_cache, m_mode);
        return service.submit_clockin(id,
                                      rest.substr(0, first),
                                      rest.substr(first + 1, second - first - 1),
                                      rest.substr(second + 1, third - second - 1),
                                      rest.substr(third + 1, fourth - third - 1) == "1",
                                      rest.substr(fourth + 1));
    }
    if (domain == "cgyy" && operation.rfind("reserve:", 0) == 0) {
        auto rest = operation.substr(8);
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            auto pos = rest.find('\n', start);
            parts.push_back(pos == std::string::npos ? rest.substr(start) : rest.substr(start, pos - start));
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        if (parts.size() < 9) {
            return make_error(ErrorCode::InvalidArgument, "cgyy reserve 参数不完整");
        }
        VenueReservationService service(m_http_client, m_cache, m_mode);
        return service.reserve(parts[0], parts[1], parts[2], id, parts[3], parts[4], parts[5], parts[6], parts[7], parts[8]);
    }
    if (domain == "cgyy" && operation == "cancel") {
        VenueReservationService service(m_http_client, m_cache, m_mode);
        return service.cancel_order(id);
    }
    if (domain == "libbook" && operation.rfind("book:", 0) == 0) {
        auto rest = operation.substr(5);
        auto sep = rest.find('\n');
        if (sep == std::string::npos) {
            return make_error(ErrorCode::InvalidArgument, "libbook book 参数不完整");
        }
        LibrarySeatService service(m_http_client, m_cache, m_mode);
        return service.reserve_seat(id, rest.substr(0, sep), rest.substr(sep + 1));
    }
    if (domain == "libbook" && operation == "cancel") {
        LibrarySeatService service(m_http_client, m_cache, m_mode);
        return service.cancel_booking(id);
    }

    return make_error(ErrorCode::NotImplemented, domain + " " + operation + " 真实写操作尚未接入");
}

#if UBAANEXT_ENABLE_MOCKS
Result<std::vector<Model::FeatureRecord>> FeatureService::mock_list(const std::string &domain, const std::string &operation) const {
    auto records = records_for(domain, operation);
    if (records.empty()) return make_error(ErrorCode::NotImplemented, domain + " " + operation + " mock 尚未接入");
    return records;
}

Result<Model::FeatureRecord> FeatureService::mock_show(const std::string &domain,
                                                       const std::string &operation,
                                                       const std::string &id) const {
    auto records = records_for(domain, operation);
    if (records.empty()) return make_error(ErrorCode::NotImplemented, domain + " " + operation + " mock 尚未接入");
    auto record = records.front();
    record.id = id;
    return record;
}
#endif

} // namespace UBAANext
