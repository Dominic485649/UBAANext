#include <UBAANext/Service/JudgeService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/JudgeParser.hpp>

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <sstream>
#include <tuple>
#include <utility>

namespace UBAANext {

namespace {

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

std::string resolve_redirect_url(const std::string &base_url, const std::string &location) {
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
        return location;
    }
    std::regex url_re(R"(^([^:]+://[^/]+)(/.*)?$)");
    std::smatch match;
    if (!std::regex_search(base_url, match, url_re)) {
        return location;
    }
    std::string authority = match[1].str();
    std::string path = match.size() > 2 ? match[2].str() : "/";
    if (location.rfind("//", 0) == 0) {
        auto colon = authority.find(':');
        return authority.substr(0, colon) + ":" + location;
    }
    if (!location.empty() && location.front() == '/') {
        return authority + location;
    }
    auto slash = path.find_last_of('/');
    std::string base_path = slash == std::string::npos ? "/" : path.substr(0, slash + 1);
    return authority + base_path + location;
}

bool is_sso_or_login_page(const HttpResponse &response) {
    if (response.status_code == 401 || response.status_code == 403) {
        return true;
    }
    const auto &body = response.body;
    return body.find("name=\"execution\"") != std::string::npos ||
           body.find("统一身份认证") != std::string::npos ||
           body.find("sso.buaa.edu.cn") != std::string::npos;
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

void apply_judge_headers(HttpRequest &request) {
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    request.headers["Accept-Language"] = "zh-CN,zh;q=0.9";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
}

Model::FeatureRecord assignment_to_record(const Model::JudgeAssignmentSummary &assignment, std::map<std::string, std::string> fields = {}) {
    fields["courseId"] = assignment.course_id;
    fields["courseName"] = assignment.course_name;
    fields["startTime"] = assignment.start_time;
    fields["dueTime"] = assignment.due_time;
    fields["maxScore"] = assignment.max_score;
    fields["myScore"] = assignment.my_score;
    fields["totalProblems"] = std::to_string(assignment.total_problems);
    fields["submittedCount"] = std::to_string(assignment.submitted_count);
    fields["submissionStatus"] = assignment.status.empty() ? "available" : assignment.status;
    fields["submissionStatusText"] = assignment.status_text;
    return make_record(assignment.id, assignment.title, assignment.status.empty() ? "available" : assignment.status, std::move(fields));
}

Model::FeatureRecord detail_to_record(const Model::JudgeAssignmentDetail &detail) {
    std::map<std::string, std::string> fields = {
        {"courseId", detail.course_id},
        {"courseName", detail.course_name},
        {"content", detail.content},
        {"startTime", detail.start_time},
        {"dueTime", detail.due_time},
        {"maxScore", detail.max_score},
        {"myScore", detail.my_score},
        {"totalProblems", std::to_string(detail.total_problems)},
        {"submittedCount", std::to_string(detail.submitted_count)},
        {"submissionStatus", detail.status},
        {"submissionStatusText", detail.status_text},
    };
    return make_record(detail.id, detail.title, detail.status, std::move(fields));
}

} // namespace

JudgeService::JudgeService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

namespace {

Result<HttpResponse> send_judge_request(IHttpClient &http_client, ConnectionMode mode, const std::string &url, const char *failure_message) {
    std::string current_url = url;
    for (int redirects = 0; redirects < 8; ++redirects) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, mode);
        apply_judge_headers(request);

        auto response = http_client.send(request);
        if (!response) return make_error(ErrorCode::NetworkError, std::string(failure_message) + ": " + response.error().message);
        if (response->status_code < 300 || response->status_code >= 400) return *response;

        auto location = header_value(*response, "Location");
        if (location.empty()) return make_error(ErrorCode::NetworkError, "希冀跳转缺少 Location");
        current_url = resolve_redirect_url(current_url, location);
    }
    return make_error(ErrorCode::NetworkError, "希冀跳转次数过多");
}

} // namespace

Result<void> JudgeService::ensure_session() {
    (void)m_cache;

    auto response = send_judge_request(m_http_client, m_mode, "https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F", "激活希冀会话失败");
    if (!response) return make_error(response.error().code, response.error().message);

    if (is_sso_or_login_page(*response)) {
        return make_error(ErrorCode::SessionExpired, "希冀会话已过期，请重新登录");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "希冀会话激活返回: " + std::to_string(response->status_code));
    }
    return {};
}

Result<std::string> JudgeService::get_html(const std::string &url) {
    auto session = ensure_session();
    if (!session) {
        return make_error(session.error().code, session.error().message);
    }

    auto response = send_judge_request(m_http_client, m_mode, url, "请求希冀失败");
    if (!response) return make_error(response.error().code, response.error().message);
    if (is_sso_or_login_page(*response)) {
        return make_error(ErrorCode::SessionExpired, "希冀会话已过期，请重新登录");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "希冀请求返回: " + std::to_string(response->status_code));
    }
    return response->body;
}

Result<std::vector<Model::JudgeAssignmentSummary>> JudgeService::list_assignment_summaries(const JudgeAssignmentQuery &query) {
    std::vector<Model::JudgeCourse> courses;
    if (!query.course_id.empty() && query.course_id != "assignments") {
        courses.push_back({query.course_id, ""});
    } else {
        auto courses_html = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=0");
        if (!courses_html) return make_error(courses_html.error().code, courses_html.error().message);
        courses = Parser::parse_judge_courses_html(*courses_html);
    }

    std::vector<Model::JudgeAssignmentSummary> records;
    for (auto &course : courses) {
        auto selected = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=" + course.id);
        if (!selected) return make_error(selected.error().code, selected.error().message);
        if (course.name.empty()) {
            auto selected_courses = Parser::parse_judge_courses_html(*selected);
            auto found = std::find_if(selected_courses.begin(), selected_courses.end(), [&](const auto &candidate) {
                return candidate.id == course.id;
            });
            if (found != selected_courses.end()) course.name = found->name;
        }

        auto html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp");
        if (!html) return make_error(html.error().code, html.error().message);
        auto assignments = Parser::parse_judge_assignments_html(*html, course);
        for (auto &assignment : assignments) {
            auto detail_html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp?assignID=" + assignment.id);
            if (!detail_html) return make_error(detail_html.error().code, detail_html.error().message);
            auto detail = Parser::parse_judge_assignment_detail_html(*detail_html, assignment);
            if (!detail) return make_error(detail.error().code, detail.error().message);
            assignment.status = assignment.status == "expired" ? "expired" : detail->status;
            assignment.start_time = detail->start_time;
            assignment.due_time = detail->due_time;
            assignment.max_score = detail->max_score;
            assignment.my_score = detail->my_score;
            assignment.total_problems = detail->total_problems;
            assignment.submitted_count = detail->submitted_count;
            assignment.status_text = detail->status_text;
            records.push_back(std::move(assignment));
        }
    }

    std::sort(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        auto lhs_due = lhs.due_time.empty() ? "9999-99-99 99:99:99" : lhs.due_time;
        auto rhs_due = rhs.due_time.empty() ? "9999-99-99 99:99:99" : rhs.due_time;
        return std::tie(lhs_due, lhs.course_name, lhs.title, lhs.id) < std::tie(rhs_due, rhs.course_name, rhs.title, rhs.id);
    });
    records.erase(std::remove_if(records.begin(), records.end(), [&](const auto &record) {
        if (!query.include_expired && record.status == "expired") return true;
        if (!query.include_history && record.status == "submitted") return true;
        return false;
    }), records.end());
    return records;
}

Result<std::vector<Model::JudgeAssignmentSummary>> JudgeService::list_assignment_summaries(const std::string &course_id) {
    JudgeAssignmentQuery query;
    query.course_id = course_id;
    return list_assignment_summaries(query);
}

Result<std::vector<Model::FeatureRecord>> JudgeService::list_assignments(const JudgeAssignmentQuery &query) {
    auto summaries = list_assignment_summaries(query);
    if (!summaries) return make_error(summaries.error().code, summaries.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &summary : *summaries) records.push_back(assignment_to_record(summary));
    return records;
}

Result<std::vector<Model::FeatureRecord>> JudgeService::list_assignments(const std::string &course_id) {
    JudgeAssignmentQuery query;
    query.course_id = course_id;
    return list_assignments(query);
}

Result<Model::JudgeAssignmentDetail> JudgeService::assignment_detail(const std::string &assignment_id) {
    if (assignment_id.empty()) return make_error(ErrorCode::InvalidArgument, "judge assignment details 需要 --assignment-id <id>");

    auto courses_html = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=0");
    if (!courses_html) return make_error(courses_html.error().code, courses_html.error().message);
    auto courses = Parser::parse_judge_courses_html(*courses_html);

    for (const auto &course : courses) {
        auto selected = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=" + course.id);
        if (!selected) return make_error(selected.error().code, selected.error().message);
        auto list_html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp");
        if (!list_html) return make_error(list_html.error().code, list_html.error().message);
        auto assignments = Parser::parse_judge_assignments_html(*list_html, course);
        auto found = std::find_if(assignments.begin(), assignments.end(), [&](const auto &assignment) {
            return assignment.id == assignment_id;
        });
        if (found == assignments.end()) continue;

        auto detail_html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp?assignID=" + assignment_id);
        if (!detail_html) return make_error(detail_html.error().code, detail_html.error().message);
        return Parser::parse_judge_assignment_detail_html(*detail_html, *found);
    }

    return make_error(ErrorCode::InvalidArgument, "未找到希冀作业: " + assignment_id);
}

Result<std::vector<Model::JudgeAssignmentDetail>> JudgeService::assignment_details_batch(const std::vector<std::string> &assignment_ids) {
    if (assignment_ids.empty()) return make_error(ErrorCode::InvalidArgument, "judge assignment details-batch 需要至少一个 assignment id");
    std::vector<Model::JudgeAssignmentDetail> records;
    for (const auto &assignment_id : assignment_ids) {
        auto detail = assignment_detail(assignment_id);
        if (!detail) return make_error(detail.error().code, detail.error().message);
        records.push_back(std::move(*detail));
    }
    return records;
}

Result<Model::FeatureRecord> JudgeService::show_assignment(const std::string &assignment_id) {
    if (assignment_id.empty()) return make_error(ErrorCode::InvalidArgument, "judge assignment show 需要 --assignment-id <id>");
    return show_assignment_details(assignment_id);
}

Result<std::vector<Model::FeatureRecord>> JudgeService::show_assignment_details_batch(const std::vector<std::string> &assignment_ids) {
    auto details = assignment_details_batch(assignment_ids);
    if (!details) return make_error(details.error().code, details.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &detail : *details) records.push_back(detail_to_record(detail));
    return records;
}

Result<Model::FeatureRecord> JudgeService::show_assignment_details(const std::string &assignment_id) {
    auto detail = assignment_detail(assignment_id);
    if (!detail) return make_error(detail.error().code, detail.error().message);
    return detail_to_record(*detail);
}

} // namespace UBAANext
