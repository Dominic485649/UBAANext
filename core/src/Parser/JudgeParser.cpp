#include <UBAANext/Parser/JudgeParser.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <tuple>
#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

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
    if (start == std::string::npos) return {};
    auto end = out.find_last_not_of(' ');
    return out.substr(start, end - start + 1);
}

std::string clean_html_text(const std::string &html) {
    return collapse_ws(strip_tags(html));
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

std::string normalize_score(std::string score) {
    auto dot = score.find('.');
    if (dot == std::string::npos) return score;
    while (!score.empty() && score.back() == '0') score.pop_back();
    if (!score.empty() && score.back() == '.') score.pop_back();
    return score;
}

std::string normalize_datetime(std::string value) {
    return std::count(value.begin(), value.end(), ':') == 1 ? value + ":00" : value;
}

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

bool contains_any(const std::string &text, const std::vector<const char *> &needles) {
    return std::any_of(needles.begin(), needles.end(), [&](const char *needle) {
        return text.find(needle) != std::string::npos;
    });
}

std::string assignment_status_from_text(const std::string &text) {
    auto lower = lower_ascii(text);
    if (contains_any(lower, {"已过期", "过期", "已截止", "已结束", "expired", "closed", "ended"})) return "expired";
    if (contains_any(lower, {"未提交", "unsubmitted", "not submitted"})) return "unsubmitted";
    if (contains_any(lower, {"已提交", "已完成", "得分", "submitted", "accepted", "completed"})) return "submitted";
    return "available";
}

void collect_judge_assignment_links(const std::string &html,
                                    const Model::JudgeCourse &course,
                                    const std::string &status,
                                    std::vector<Model::JudgeAssignmentSummary> &records) {
    std::regex link_re(R"JUDGE(<a\b[^>]*href\s*=\s*(?:"([^"]*assignID=(\d+)[^"]*)"|'([^']*assignID=(\d+)[^']*)'|([^\s>]*assignID=(\d+)[^\s>]*))[^>]*>([\s\S]*?)</a>)JUDGE", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), link_re), end; it != end; ++it) {
        const auto &match = *it;
        std::string href = match[1].matched ? match[1].str() : match[3].matched ? match[3].str() : match[5].str();
        if (href.find("problemContent") != std::string::npos || href.find("judgeDetails") != std::string::npos) continue;
        std::string assignment_id = match[2].matched ? match[2].str() : match[4].matched ? match[4].str() : match[6].str();
        std::string title = clean_html_text(match[7].str());
        if (assignment_id.empty() || title.empty()) continue;
        Model::JudgeAssignmentSummary record;
        record.id = assignment_id;
        record.course_id = course.id;
        record.course_name = course.name;
        record.title = title;
        record.status = status.empty() ? "available" : status;
        records.push_back(std::move(record));
    }
}

} // namespace

std::vector<Model::JudgeCourse> parse_judge_courses_html(const std::string &html) {
    std::vector<Model::JudgeCourse> courses;
    std::regex link_re(R"JUDGE(<a\b[^>]*href\s*=\s*(?:"[^"]*courselist\.jsp\?courseID=(\d+)[^"]*"|'[^']*courselist\.jsp\?courseID=(\d+)[^']*'|[^\s>]*courselist\.jsp\?courseID=(\d+)[^\s>]*)[^>]*>([\s\S]*?)</a>)JUDGE", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), link_re), end; it != end; ++it) {
        const auto &match = *it;
        std::string course_id = match[1].matched ? match[1].str() : match[2].matched ? match[2].str() : match[3].str();
        std::string course_name = clean_html_text(match[4].str());
        if (course_id.empty() || course_id == "0" || course_name.empty()) continue;
        courses.push_back({course_id, course_name});
    }
    std::sort(courses.begin(), courses.end(), [](const auto &lhs, const auto &rhs) { return lhs.id < rhs.id; });
    courses.erase(std::unique(courses.begin(), courses.end(), [](const auto &lhs, const auto &rhs) { return lhs.id == rhs.id; }), courses.end());
    return courses;
}

std::vector<Model::JudgeAssignmentSummary> parse_judge_assignments_html(const std::string &html,
                                                                        const Model::JudgeCourse &course) {
    std::vector<Model::JudgeAssignmentSummary> records;
    std::regex row_re(R"JUDGE(<tr\b[^>]*>([\s\S]*?)</tr>)JUDGE", std::regex::icase);
    for (std::sregex_iterator it(html.begin(), html.end(), row_re), end; it != end; ++it) {
        const auto row_html = (*it)[1].str();
        collect_judge_assignment_links(row_html, course, assignment_status_from_text(clean_html_text(row_html)), records);
    }
    if (records.empty()) {
        collect_judge_assignment_links(html, course, "available", records);
    }
    std::sort(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        return std::tie(lhs.course_id, lhs.id) < std::tie(rhs.course_id, rhs.id);
    });
    records.erase(std::unique(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.course_id == rhs.course_id && lhs.id == rhs.id;
    }), records.end());
    return records;
}

Result<Model::JudgeAssignmentDetail> parse_judge_assignment_detail_html(const std::string &html,
                                                                        const Model::JudgeAssignmentSummary &summary) {
    auto text = clean_html_text(html);
    Model::JudgeAssignmentDetail detail;
    detail.id = summary.id;
    detail.course_id = summary.course_id;
    detail.course_name = summary.course_name;
    detail.title = summary.title;
    detail.content = text;

    std::regex time_re(R"JUDGE(作业时间[^\d]*(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}(?::\d{2})?)\s*至\s*(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}(?::\d{2})?))JUDGE");
    std::smatch time_match;
    if (std::regex_search(text, time_match, time_re)) {
        detail.start_time = normalize_datetime(time_match[1].str());
        detail.due_time = normalize_datetime(time_match[2].str());
    }
    std::regex max_score_re(R"JUDGE(作业满分[^\d]*([\d.]+))JUDGE");
    std::smatch max_score_match;
    if (std::regex_search(text, max_score_match, max_score_re)) detail.max_score = normalize_score(max_score_match[1].str());
    std::regex my_score_re(R"JUDGE(总分[^\d]*([\d.]+))JUDGE");
    std::smatch my_score_match;
    if (std::regex_search(text, my_score_match, my_score_re)) detail.my_score = normalize_score(my_score_match[1].str());
    std::regex total_re(R"JUDGE(共\s*(\d+)\s*道)JUDGE");
    std::smatch total_match;
    if (std::regex_search(text, total_match, total_re)) detail.total_problems = std::stoi(total_match[1].str());
    detail.submitted_count = count_occurrences(text, "得分：") + count_occurrences(text, "得分:");
    if (detail.submitted_count == 0) {
        detail.submitted_count = count_occurrences(text, "最后一次提交时间") + count_occurrences(text, "初次提交时间");
    }
    if (detail.total_problems > 0 && detail.submitted_count > detail.total_problems) detail.submitted_count = detail.total_problems;
    detail.status = detail.total_problems <= 0 ? "unknown" : detail.submitted_count <= 0 ? "unsubmitted" : detail.submitted_count < detail.total_problems ? "partial" : "submitted";
    detail.status_text = submission_status_text(detail.submitted_count, detail.total_problems, detail.my_score, detail.max_score);
    return detail;
}

} // namespace Parser
} // namespace UBAANext
