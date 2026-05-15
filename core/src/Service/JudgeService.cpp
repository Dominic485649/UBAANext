#include <UBAANext/Service/JudgeService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>

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

std::string html_unescape(std::string text) {
    const std::pair<const char *, const char *> replacements[] = {
        {"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&#x27;", "'"},
    };
    for (const auto &[from, to] : replacements) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, std::char_traits<char>::length(from), to);
            pos += std::char_traits<char>::length(to);
        }
    }
    return text;
}

std::string strip_tags(const std::string &html) {
    return std::regex_replace(html, std::regex(R"(<[^>]+>)"), " ");
}

std::string collapse_ws(std::string text) {
    text = html_unescape(std::move(text));
    std::string out;
    bool last_space = false;
    for (char ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isspace(uch)) {
            if (!last_space) {
                out.push_back(' ');
                last_space = true;
            }
        } else {
            out.push_back(ch);
            last_space = false;
        }
    }
    auto start = out.find_first_not_of(' ');
    if (start == std::string::npos) {
        return {};
    }
    auto end = out.find_last_not_of(' ');
    return out.substr(start, end - start + 1);
}

std::string clean_html_text(const std::string &html) {
    return collapse_ws(strip_tags(html));
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

struct JudgeCourse {
    std::string id;
    std::string name;
};

struct JudgeAssignment {
    std::string id;
    std::string course_id;
    std::string course_name;
    std::string title;
};

void apply_judge_headers(HttpRequest &request) {
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
    request.headers["Accept-Language"] = "zh-CN,zh;q=0.9";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
}

std::vector<JudgeCourse> parse_courses(const std::string &html) {
    std::vector<JudgeCourse> courses;
    std::regex link_re(R"JUDGE(<a\b[^>]*href\s*=\s*(?:"[^"]*courselist\.jsp\?courseID=(\d+)[^"]*"|'[^']*courselist\.jsp\?courseID=(\d+)[^']*'|[^\s>]*courselist\.jsp\?courseID=(\d+)[^\s>]*)[^>]*>([\s\S]*?)</a>)JUDGE", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), link_re), end; it != end; ++it) {
        const auto &match = *it;
        std::string course_id = match[1].matched ? match[1].str() : match[2].matched ? match[2].str() : match[3].str();
        std::string course_name = clean_html_text(match[4].str());
        if (course_id.empty() || course_id == "0" || course_name.empty()) {
            continue;
        }
        courses.push_back({course_id, course_name});
    }
    std::sort(courses.begin(), courses.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.id < rhs.id;
    });
    courses.erase(std::unique(courses.begin(), courses.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.id == rhs.id;
    }), courses.end());
    return courses;
}

int count_occurrences(const std::string &text, const std::string &needle) {
    if (needle.empty()) return 0;
    int count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

std::string submission_status_text(int submitted, int total, const std::string &my_score, const std::string &max_score) {
    if (total <= 0) return "未知状态";
    if (submitted <= 0) return "未提交";
    if (submitted < total) return "进行中(" + std::to_string(submitted) + "/" + std::to_string(total) + ")";
    if (!my_score.empty() && !max_score.empty()) return "已完成 " + my_score + "/" + max_score;
    return "已完成";
}

std::vector<JudgeAssignment> parse_assignments(const std::string &html, const JudgeCourse &course) {
    std::vector<JudgeAssignment> records;
    std::regex link_re(R"JUDGE(<a\b[^>]*href\s*=\s*(?:"([^"]*assignID=(\d+)[^"]*)"|'([^']*assignID=(\d+)[^']*)'|([^\s>]*assignID=(\d+)[^\s>]*))[^>]*>([\s\S]*?)</a>)JUDGE", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), link_re), end; it != end; ++it) {
        const auto &match = *it;
        std::string href = match[1].matched ? match[1].str() : match[3].matched ? match[3].str() : match[5].str();
        if (href.find("problemContent") != std::string::npos || href.find("judgeDetails") != std::string::npos) {
            continue;
        }
        std::string assignment_id = match[2].matched ? match[2].str() : match[4].matched ? match[4].str() : match[6].str();
        std::string title = clean_html_text(match[7].str());
        if (assignment_id.empty() || title.empty()) {
            continue;
        }
        records.push_back({assignment_id, course.id, course.name, title});
    }
    std::sort(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        return std::tie(lhs.course_id, lhs.id) < std::tie(rhs.course_id, rhs.id);
    });
    records.erase(std::unique(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.course_id == rhs.course_id && lhs.id == rhs.id;
    }), records.end());
    return records;
}

Model::FeatureRecord assignment_to_record(const JudgeAssignment &assignment, std::map<std::string, std::string> fields = {}) {
    fields["courseId"] = assignment.course_id;
    fields["courseName"] = assignment.course_name;
    return make_record(assignment.id, assignment.title, "available", std::move(fields));
}

} // namespace

JudgeService::JudgeService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<void> JudgeService::ensure_session() {
    (void)m_cache;

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F", m_mode);
    apply_judge_headers(request);

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "激活希冀会话失败: " + response.error().message);
    }

    if (response->status_code >= 300 && response->status_code < 400) {
        auto location = header_value(*response, "Location");
        if (location.empty()) {
            return make_error(ErrorCode::NetworkError, "希冀登录跳转缺少 Location");
        }
        HttpRequest redirect;
        redirect.method = HttpMethod::Get;
        redirect.url = resolve_for_mode(resolve_redirect_url("https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F", location), m_mode);
        apply_judge_headers(redirect);
        response = m_http_client.send(redirect);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "跟随希冀登录跳转失败: " + response.error().message);
        }
    }

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

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    apply_judge_headers(request);

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求希冀失败: " + response.error().message);
    }
    if (is_sso_or_login_page(*response)) {
        return make_error(ErrorCode::SessionExpired, "希冀会话已过期，请重新登录");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "希冀请求返回: " + std::to_string(response->status_code));
    }
    return response->body;
}

Result<std::vector<Model::FeatureRecord>> JudgeService::list_assignments(const JudgeAssignmentQuery &query) {
    std::vector<JudgeCourse> courses;
    if (!query.course_id.empty() && query.course_id != "assignments") {
        courses.push_back({query.course_id, ""});
    } else {
        auto courses_html = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=0");
        if (!courses_html) {
            return make_error(courses_html.error().code, courses_html.error().message);
        }
        courses = parse_courses(*courses_html);
    }

    std::vector<Model::FeatureRecord> records;
    for (auto &course : courses) {
        auto selected = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=" + course.id);
        if (!selected) {
            return make_error(selected.error().code, selected.error().message);
        }
        if (course.name.empty()) {
            auto selected_courses = parse_courses(*selected);
            auto found = std::find_if(selected_courses.begin(), selected_courses.end(), [&](const auto &candidate) {
                return candidate.id == course.id;
            });
            if (found != selected_courses.end()) {
                course.name = found->name;
            }
        }

        auto html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp");
        if (!html) {
            return make_error(html.error().code, html.error().message);
        }
        for (const auto &assignment : parse_assignments(*html, course)) {
            records.push_back(assignment_to_record(assignment));
        }
    }

    std::sort(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        return std::tie(lhs.fields.at("courseName"), lhs.title, lhs.id) < std::tie(rhs.fields.at("courseName"), rhs.title, rhs.id);
    });
    if (!query.include_history) {
        records.erase(std::remove_if(records.begin(), records.end(), [](const auto &record) {
            auto status = record.fields.find("submissionStatus");
            return status != record.fields.end() && status->second == "submitted";
        }), records.end());
    }
    if (!query.include_expired) {
        records.erase(std::remove_if(records.begin(), records.end(), [](const auto &record) {
            auto status = record.fields.find("expired");
            return status != record.fields.end() && status->second == "true";
        }), records.end());
    }
    return records;
}

Result<std::vector<Model::FeatureRecord>> JudgeService::list_assignments(const std::string &course_id) {
    JudgeAssignmentQuery query;
    query.course_id = course_id;
    return list_assignments(query);
}

Result<Model::FeatureRecord> JudgeService::show_assignment(const std::string &assignment_id) {
    if (assignment_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "judge assignment show 需要 --assignment-id <id>");
    }
    return show_assignment_details(assignment_id);
}

Result<std::vector<Model::FeatureRecord>> JudgeService::show_assignment_details_batch(const std::vector<std::string> &assignment_ids) {
    if (assignment_ids.empty()) {
        return make_error(ErrorCode::InvalidArgument, "judge assignment details-batch 需要至少一个 assignment id");
    }

    std::vector<Model::FeatureRecord> records;
    for (const auto &assignment_id : assignment_ids) {
        auto detail = show_assignment_details(assignment_id);
        if (!detail) {
            return make_error(detail.error().code, detail.error().message);
        }
        records.push_back(std::move(*detail));
    }
    return records;
}

Result<Model::FeatureRecord> JudgeService::show_assignment_details(const std::string &assignment_id) {
    if (assignment_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "judge assignment details 需要 --assignment-id <id>");
    }

    auto courses_html = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=0");
    if (!courses_html) {
        return make_error(courses_html.error().code, courses_html.error().message);
    }
    auto courses = parse_courses(*courses_html);

    for (const auto &course : courses) {
        auto selected = get_html("https://judge.buaa.edu.cn/courselist.jsp?courseID=" + course.id);
        if (!selected) {
            return make_error(selected.error().code, selected.error().message);
        }
        auto list_html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp");
        if (!list_html) {
            return make_error(list_html.error().code, list_html.error().message);
        }
        auto assignments = parse_assignments(*list_html, course);
        auto found = std::find_if(assignments.begin(), assignments.end(), [&](const auto &assignment) {
            return assignment.id == assignment_id;
        });
        if (found == assignments.end()) {
            continue;
        }

        auto detail_html = get_html("https://judge.buaa.edu.cn/assignment/index.jsp?assignID=" + assignment_id);
        if (!detail_html) {
            return make_error(detail_html.error().code, detail_html.error().message);
        }
        auto text = clean_html_text(*detail_html);
        std::map<std::string, std::string> fields = {
            {"courseId", found->course_id},
            {"courseName", found->course_name},
            {"content", text},
        };
        std::regex time_re(R"JUDGE(作业时间[：:]\s*(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}(?::\d{2})?)\s*至\s*(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}(?::\d{2})?))JUDGE");
        std::smatch time_match;
        if (std::regex_search(text, time_match, time_re)) {
            fields["startTime"] = time_match[1].str();
            fields["dueTime"] = time_match[2].str();
        }
        std::regex max_score_re(R"JUDGE(作业满分[：:]\s*([\d.]+))JUDGE");
        std::smatch max_score_match;
        if (std::regex_search(text, max_score_match, max_score_re)) {
            fields["maxScore"] = max_score_match[1].str();
        }
        std::regex my_score_re(R"JUDGE(总分[：:]\s*([\d.]+))JUDGE");
        std::smatch my_score_match;
        if (std::regex_search(text, my_score_match, my_score_re)) {
            fields["myScore"] = my_score_match[1].str();
        }
        int total_problems = 0;
        std::regex total_re(R"JUDGE(共\s*(\d+)\s*道)JUDGE");
        std::smatch total_match;
        if (std::regex_search(text, total_match, total_re)) {
            fields["totalProblems"] = total_match[1].str();
            total_problems = std::stoi(total_match[1].str());
        }
        int submitted_count = count_occurrences(text, "得分：") + count_occurrences(text, "得分:") + count_occurrences(text, "最后一次提交时间") + count_occurrences(text, "初次提交时间");
        if (total_problems > 0 && submitted_count > total_problems) submitted_count = total_problems;
        fields["submittedCount"] = std::to_string(submitted_count);
        fields["submissionStatus"] = total_problems <= 0 ? "unknown" : submitted_count <= 0 ? "unsubmitted" : submitted_count < total_problems ? "partial" : "submitted";
        fields["submissionStatusText"] = submission_status_text(submitted_count, total_problems, fields["myScore"], fields["maxScore"]);
        return make_record(found->id, found->title, fields["submissionStatus"], std::move(fields));
    }

    return make_error(ErrorCode::InvalidArgument, "未找到希冀作业: " + assignment_id);
}

} // namespace UBAANext
