#include <UBAANext/Parser/SpocParser.hpp>

#include <cctype>
#include <regex>
#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    return {};
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

std::string clean_html_text(const std::string &html) {
    auto text = std::regex_replace(html, std::regex(R"(<br\s*/?>)", std::regex::icase), " ");
    text = std::regex_replace(text, std::regex(R"(<[^>]+>)"), " ");
    text = html_unescape(std::move(text));
    std::string out;
    bool last_space = false;
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            if (!last_space) out.push_back(' ');
            last_space = true;
        } else {
            out.push_back(static_cast<char>(ch));
            last_space = false;
        }
    }
    auto start = out.find_first_not_of(' ');
    if (start == std::string::npos) return {};
    auto end = out.find_last_not_of(' ');
    return out.substr(start, end - start + 1);
}

std::string normalize_status(const std::string &raw_status) {
    return raw_status == "1" || raw_status == "已做" || raw_status == "已提交" ? "submitted" : "unsubmitted";
}

} // namespace

std::vector<Model::SpocAssignmentSummary> parse_spoc_assignments_page(const nlohmann::json &page,
                                                                      const std::map<std::string, std::pair<std::string, std::string>> &courses,
                                                                      const std::string &term_code,
                                                                      const std::string &term_name) {
    auto list = page.contains("list") && page["list"].is_array() ? page["list"] : nlohmann::json::array();
    std::vector<Model::SpocAssignmentSummary> records;
    for (const auto &assignment : list) {
        Model::SpocAssignmentSummary record;
        record.id = json_string(assignment, "zyid");
        record.course_id = json_string(assignment, "sskcid");
        auto course_it = courses.find(record.course_id);
        record.course_name = json_string(assignment, "kcmc");
        if (course_it != courses.end()) {
            if (record.course_name.empty()) record.course_name = course_it->second.first;
            record.teacher = course_it->second.second;
        }
        record.title = json_string(assignment, "zymc");
        record.submission_status = json_string(assignment, "tjzt");
        record.status = normalize_status(record.submission_status);
        record.start_time = json_string(assignment, "zykssj");
        record.due_time = json_string(assignment, "zyjzsj");
        record.score = json_string(assignment, "mf");
        record.term_code = term_code;
        record.term_name = term_name;
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

Model::SpocAssignmentDetail parse_spoc_assignment_detail(const nlohmann::json &detail,
                                                         const nlohmann::json *submission,
                                                         const std::string &assignment_id) {
    Model::SpocAssignmentDetail record;
    record.id = assignment_id;
    record.course_id = json_string(detail, "sskcid");
    record.title = json_string(detail, "zymc");
    if (record.title.empty()) record.title = assignment_id;
    record.start_time = json_string(detail, "zykssj");
    record.due_time = json_string(detail, "zyjzsj");
    record.score = json_string(detail, "zyfs");
    record.content = clean_html_text(json_string(detail, "zynr"));
    record.status = "unknown";
    if (submission != nullptr) {
        record.submission_status = json_string(*submission, "tjzt");
        record.submitted_at = json_string(*submission, "tjsj");
        record.status = normalize_status(record.submission_status);
    }
    return record;
}

} // namespace Parser
} // namespace UBAANext
