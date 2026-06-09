#include <UBAANext/Service/FeatureService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Version.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Service/ClassroomService.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Service/TodoService.hpp>
#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Service/YgdkService.hpp>

#include <nlohmann/json.hpp>

#include <charconv>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace UBAANext {

namespace {

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

bool response_is_sso(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response);
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

std::string join_ints(const std::vector<int> &values) {
    std::string text;
    for (int value : values) {
        if (!text.empty()) text += ",";
        text += std::to_string(value);
    }
    return text;
}

std::vector<Model::FeatureRecord> course_records(const std::vector<Model::Course> &courses) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(courses.size());
    for (const auto &course : courses) {
        records.push_back(make_record(course.id.empty() ? course.name : course.id,
                                      course.name,
                                      "scheduled",
                                      {{"teacher", course.teacher},
                                       {"classroom", course.classroom},
                                       {"dayOfWeek", std::to_string(course.day_of_week)},
                                       {"section", std::to_string(course.section_start) + "-" + std::to_string(course.section_end)},
                                       {"week", std::to_string(course.week_start) + "-" + std::to_string(course.week_end)},
                                       {"time", course.begin_time + "-" + course.end_time}}));
    }
    return records;
}

std::vector<Model::FeatureRecord> exam_records(const std::vector<Model::Exam> &exams) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(exams.size());
    for (const auto &exam : exams) {
        records.push_back(make_record(exam.id.empty() ? exam.course_name : exam.id,
                                      exam.course_name,
                                      exam.exam_type.empty() ? "exam" : exam.exam_type,
                                      {{"location", exam.location},
                                       {"time", exam.time_text.empty() ? exam.exam_date + " " + exam.start_time + "-" + exam.end_time : exam.time_text},
                                       {"seat", exam.seat_no},
                                       {"courseNo", exam.course_no}}));
    }
    return records;
}

std::vector<Model::FeatureRecord> classroom_records(const Model::ClassroomQueryResult &classrooms) {
    std::vector<Model::FeatureRecord> records;
    for (const auto &[building, items] : classrooms.buildings) {
        for (const auto &room : items) {
            records.push_back(make_record(room.id.empty() ? building + ":" + room.name : room.id,
                                          building + " " + room.name,
                                          "free",
                                          {{"building", building}, {"floor", room.floor_id}, {"sections", join_ints(room.free_sections)}}));
        }
    }
    return records;
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

std::string json_to_string(const nlohmann::json &value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<int>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned int>());
    if (value.is_number_float()) return std::to_string(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return {};
}

std::string pick_string(const nlohmann::json &value, std::initializer_list<const char *> keys) {
    if (!value.is_object()) return {};
    for (const auto *key : keys) {
        auto it = value.find(key);
        if (it != value.end()) {
            auto text = json_to_string(*it);
            if (!text.empty()) return text;
        }
    }
    return {};
}

const nlohmann::json *find_array_payload(const nlohmann::json &json) {
    if (json.is_array()) return &json;
    if (!json.is_object()) return nullptr;
    for (const auto *key : {"data", "datas", "list", "rows", "items", "result"}) {
        auto it = json.find(key);
        if (it == json.end()) continue;
        if (it->is_array()) return &*it;
        if (it->is_object()) {
            if (auto nested = find_array_payload(*it)) return nested;
        }
    }
    return nullptr;
}

std::vector<Model::FeatureRecord> parse_announcement_records(const std::string &body) {
    auto json = nlohmann::json::parse(body);
    auto payload = find_array_payload(json);
    if (payload == nullptr) return {};

    std::vector<Model::FeatureRecord> records;
    for (std::size_t i = 0; i < payload->size(); ++i) {
        const auto &item = (*payload)[i];
        auto id = pick_string(item, {"id", "noticeId", "announcementId", "uuid"});
        auto title = pick_string(item, {"title", "name", "subject", "bt"});
        auto status = pick_string(item, {"status", "state", "publishStatus"});
        std::map<std::string, std::string> fields;
        if (item.is_object()) {
            for (const auto &[key, value] : item.items()) {
                auto text = json_to_string(value);
                if (!text.empty()) fields[key] = text;
            }
        }
        if (id.empty()) id = "announcement-" + std::to_string(i + 1);
        if (title.empty()) title = "公告";
        if (status.empty()) status = "published";
        records.push_back(make_record(std::move(id), std::move(title), std::move(status), std::move(fields)));
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> fetch_announcements(IHttpClient &http_client, ConnectionMode mode) {
    constexpr const char *kAnnouncementsUrl = "https://app.buaa.edu.cn/uc/wap/notice/list";

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(kAnnouncementsUrl, mode);
    request.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["User-Agent"] = "UBAANext/0.4";

    auto response = http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求公告失败: " + response.error().message);
    }
    if (response_is_sso(*response)) {
        return make_error(ErrorCode::SessionExpired, "公告会话已过期");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "公告请求返回: " + std::to_string(response->status_code));
    }

    try {
        return parse_announcement_records(response->body);
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析公告 JSON 失败: ") + e.what());
    }
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

Result<std::vector<int>> parse_sections(const std::string &text) {
    std::vector<int> sections;
    std::string item;
    std::istringstream input(text);
    while (std::getline(input, item, ',')) {
        auto section = parse_positive_int(item, "classroom section");
        if (!section) return make_error(section.error().code, section.error().message);
        sections.push_back(*section);
    }
    return sections;
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
    if (domain == "live") {
        return {make_record("live-1", "课堂直播", "scheduled", {{"operation", operation}, {"source", "mock"}})};
    }
    if (domain == "file" || domain == "cloud") {
        if (operation.rfind("roots", 0) == 0) {
            return {make_record("cloud-root-user", "个人文档库", "root", {{"type", "user_doc_lib"}, {"docLibName", "个人文档库"}, {"source", "mock"}, {"operation", operation}}),
                    make_record("cloud-root-shared", "共享文档库", "root", {{"type", "shared_user_doc_lib"}, {"docLibName", "共享文档库"}, {"source", "mock"}, {"operation", operation}})};
        }
        if (operation == "root") {
            return {make_record("cloud-root-user", "个人文档库", "root", {{"type", "user_doc_lib"}, {"docLibName", "个人文档库"}, {"source", "mock"}})};
        }
        if (operation == "list") {
            return {make_record("cloud-dir-1", "示例文件夹", "dir", {{"type", "dir"}, {"size", "-1"}, {"parentId", "cloud-root-user"}, {"source", "mock"}}),
                    make_record("cloud-file-1", "示例文件.txt", "file", {{"type", "file"}, {"size", "1024"}, {"parentId", "cloud-root-user"}, {"source", "mock"}})};
        }
        if (operation == "size") {
            return {make_record("cloud-size", "Cloud item size", "ok", {{"bytes", "1024"}, {"fileCount", "1"}, {"dirCount", "1"}, {"source", "mock"}})};
        }
        if (operation == "recycle") {
            return {make_record("cloud-recycle-1", "回收站示例文件", "recycle-file", {{"type", "file"}, {"size", "512"}, {"source", "mock"}})};
        }
        if (operation == "shares") {
            return {make_record("cloud-share-1", "示例分享", "shared", {{"itemId", "cloud-file-1"}, {"expiresAt", "never"}, {"permissions", "read"}, {"source", "mock"}})};
        }
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
            return fetch_announcements(m_http_client, m_mode);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的公告查询操作: " + operation);
    }

    if (domain == "course") {
        CourseService service(m_http_client, m_cache, m_mode);
        if (operation == "today") {
            auto courses = service.get_today_courses();
            if (!courses) return make_error(courses.error().code, courses.error().message);
            return course_records(*courses);
        }
        if (operation.rfind("date:", 0) == 0) {
            auto courses = service.get_date_courses(operation.substr(5));
            if (!courses) return make_error(courses.error().code, courses.error().message);
            return course_records(*courses);
        }
        if (operation.rfind("week", 0) == 0) {
            auto parts = operation == "week" ? std::vector<std::string>{} : split_colon_fields(operation.substr(5));
            auto week_text = parts.empty() ? std::string{"1"} : parts[0];
            auto week = parse_positive_int(week_text, "course week");
            if (!week) return make_error(week.error().code, week.error().message);
            auto courses = parts.size() > 1 && !parts[1].empty() ? service.get_week_courses(*week, parts[1]) : service.get_week_courses(*week);
            if (!courses) return make_error(courses.error().code, courses.error().message);
            return course_records(*courses);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的课程查询操作: " + operation);
    }

    if (domain == "exam") {
        ExamService service(m_http_client, m_cache, m_mode);
        auto term = operation.rfind("list:", 0) == 0 ? operation.substr(5) : std::string{};
        if (operation == "list" || operation.rfind("list:", 0) == 0) {
            auto exams = service.get_exams(term);
            if (!exams) return make_error(exams.error().code, exams.error().message);
            return exam_records(*exams);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的考试查询操作: " + operation);
    }

    if (domain == "classroom") {
        if (operation.rfind("query:", 0) != 0) {
            return make_error(ErrorCode::InvalidArgument, "空教室查询需要 query:<campus>:<date>[:sections]");
        }
        auto parts = split_colon_fields(operation.substr(6));
        if (parts.size() < 2) {
            return make_error(ErrorCode::InvalidArgument, "空教室查询需要校区和日期");
        }
        auto campus = parse_positive_int(parts[0], "classroom campus");
        if (!campus) return make_error(campus.error().code, campus.error().message);
        ClassroomService service(m_http_client, m_cache, m_mode);
        Result<Model::ClassroomQueryResult> classrooms = parts.size() > 2 && !parts[2].empty()
            ? [&]() -> Result<Model::ClassroomQueryResult> {
                  auto sections = parse_sections(parts[2]);
                  if (!sections) return make_error(sections.error().code, sections.error().message);
                  return service.query_classrooms(*campus, parts[1], *sections);
              }()
            : service.query_classrooms(*campus, parts[1]);
        if (!classrooms) return make_error(classrooms.error().code, classrooms.error().message);
        return classroom_records(*classrooms);
    }

    if (domain == "todo") {
        TodoService service(m_http_client, m_cache, m_mode);
        TodoQuery query;
        query.pending_only = operation != "all";
        if (operation == "list" || operation == "pending" || operation == "all") {
            return service.list_todos(query);
        }
        return make_error(ErrorCode::InvalidArgument, "未知的待办查询操作: " + operation);
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
                                                     [[maybe_unused]] const std::string &id,
                                                     bool confirmed) {
    if (!confirmed) {
        return make_error(ErrorCode::InvalidArgument, domain + " " + operation + " 是有副作用操作，必须通过 --confirm、--yes 或 -y 确认");
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

    return make_error(ErrorCode::UnsupportedPlatform, domain + " " + operation + " 真实写操作必须通过 typed service 写门控执行");
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
