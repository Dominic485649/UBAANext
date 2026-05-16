#include <UBAANext/Parser/SpocParser.hpp>

#include <algorithm>
#include <cctype>
#include <ctime>
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

std::string trim_ascii(std::string text) {
    auto begin = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) { return std::isspace(ch); });
    auto end = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    return begin >= end ? std::string{} : std::string(begin, end);
}

std::string lower_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string normalize_score(const std::string &raw_score) {
    auto normalized = trim_ascii(raw_score);
    if (normalized.empty()) return {};
    std::smatch match;
    if (std::regex_search(normalized, match, std::regex(R"(-?\d+(?:\.\d+)?)")) && !match.empty()) return match[0].str();
    return normalized;
}

std::string normalize_datetime(std::string raw_value) {
    auto normalized = trim_ascii(std::move(raw_value));
    if (normalized.empty()) return {};
    std::smatch match;
    std::regex iso_re(R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(?:([+-])(\d{2}):(\d{2})|Z)$)");
    if (std::regex_match(normalized, match, iso_re)) {
        std::tm tm{};
        tm.tm_year = std::stoi(match[1].str()) - 1900;
        tm.tm_mon = std::stoi(match[2].str()) - 1;
        tm.tm_mday = std::stoi(match[3].str());
        tm.tm_hour = std::stoi(match[4].str());
        tm.tm_min = std::stoi(match[5].str());
        tm.tm_sec = std::stoi(match[6].str());
#ifdef _WIN32
        auto seconds = _mkgmtime(&tm);
#else
        auto seconds = timegm(&tm);
#endif
        if (seconds != static_cast<std::time_t>(-1)) {
            if (match[7].matched) {
                auto offset = std::stoi(match[8].str()) * 3600 + std::stoi(match[9].str()) * 60;
                seconds += match[7].str() == "+" ? -offset : offset;
            }
            seconds += 8 * 3600;
            std::tm local{};
#ifdef _WIN32
            gmtime_s(&local, &seconds);
#else
            gmtime_r(&seconds, &local);
#endif
            char buffer[20]{};
            if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local) > 0) return buffer;
        }
    }
    std::replace(normalized.begin(), normalized.end(), 'T', ' ');
    auto dot = normalized.find('.');
    if (dot != std::string::npos) normalized = normalized.substr(0, dot);
    return normalized;
}

std::string normalize_status(const std::string &raw_status, bool has_content) {
    auto trimmed = trim_ascii(raw_status);
    auto lower = lower_ascii(trimmed);
    if (trimmed == "1" || trimmed == "已做" || trimmed == "已提交" || lower == "submitted" || lower == "completed") return "submitted";
    if (trimmed == "0" || trimmed == "未做" || trimmed == "未提交" || lower == "unsubmitted" || lower == "pending") return "unsubmitted";
    if (trimmed == "已过期" || trimmed == "过期" || lower == "expired" || lower == "closed") return "expired";
    if (!has_content) return "unsubmitted";
    return "unknown";
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
        record.status = normalize_status(record.submission_status, !record.submission_status.empty());
        record.start_time = normalize_datetime(json_string(assignment, "zykssj"));
        record.due_time = normalize_datetime(json_string(assignment, "zyjzsj"));
        record.score = normalize_score(json_string(assignment, "mf"));
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
    record.start_time = normalize_datetime(json_string(detail, "zykssj"));
    record.due_time = normalize_datetime(json_string(detail, "zyjzsj"));
    record.score = normalize_score(json_string(detail, "zyfs"));
    record.content = clean_html_text(json_string(detail, "zynr"));
    record.status = "unknown";
    if (submission != nullptr) {
        record.submission_status = json_string(*submission, "tjzt");
        record.submitted_at = normalize_datetime(json_string(*submission, "tjsj"));
        record.status = normalize_status(record.submission_status, true);
    }
    return record;
}

} // namespace Parser
} // namespace UBAANext
