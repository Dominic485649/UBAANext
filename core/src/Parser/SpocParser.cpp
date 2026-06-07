#include <UBAANext/Parser/SpocParser.hpp>

#include <UBAANext/Base/TimeUtils.hpp>

#include <algorithm>
#include <charconv>
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

bool parse_int(const std::string &value, int &out) {
    auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    return !value.empty() && result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

std::string normalize_datetime(std::string raw_value) {
    auto normalized = trim_ascii(std::move(raw_value));
    if (normalized.empty()) return {};
    std::smatch match;
    std::regex iso_re(R"(^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(?:([+-])(\d{2}):(\d{2})|Z)$)");
    if (std::regex_match(normalized, match, iso_re)) {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;
        if (!parse_int(match[1].str(), year) || !parse_int(match[2].str(), month) || !parse_int(match[3].str(), day) ||
            !parse_int(match[4].str(), hour) || !parse_int(match[5].str(), minute) || !parse_int(match[6].str(), second)) {
            return normalized;
        }
        std::tm tm{};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        auto seconds = utc_time_t(tm);
        if (seconds != static_cast<std::time_t>(-1)) {
            if (match[7].matched) {
                int offset_hour = 0;
                int offset_minute = 0;
                if (!parse_int(match[8].str(), offset_hour) || !parse_int(match[9].str(), offset_minute)) return normalized;
                auto offset = offset_hour * 3600 + offset_minute * 60;
                seconds += match[7].str() == "+" ? -offset : offset;
            }
            seconds += 8 * 3600;
            auto local = utc_time(seconds);
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

std::pair<std::string, std::string> parse_spoc_date_range(const std::string &raw_value) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : raw_value) {
        if (ch == ',') {
            parts.push_back(trim_ascii(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty() || !raw_value.empty()) parts.push_back(trim_ascii(current));
    if (parts.size() >= 3) return {parts[1], parts[2]};
    if (parts.size() >= 2) return {parts[0], parts[1]};
    return {};
}

std::pair<std::string, std::string> parse_spoc_time_range(const std::string &raw_value) {
    auto text = trim_ascii(raw_value);
    if (text.empty()) return {};
    auto space = text.find(' ');
    auto dash = text.find('-', space == std::string::npos ? 0 : space + 1);
    if (space == std::string::npos || dash == std::string::npos) return {};
    const auto date = text.substr(0, space);
    return {normalize_datetime(date + " " + text.substr(space + 1, dash - space - 1)),
            normalize_datetime(date + " " + text.substr(dash + 1))};
}

} // namespace

Model::SpocWeek parse_spoc_week(const nlohmann::json &content) {
    Model::SpocWeek week;
    week.term_code = json_string(content, "mrxq");
    week.raw_date_range = json_string(content, "pjmrrq");
    auto [start, end] = parse_spoc_date_range(week.raw_date_range);
    week.start_date = std::move(start);
    week.end_date = std::move(end);
    return week;
}

std::vector<Model::SpocSchedule> parse_spoc_schedule(const nlohmann::json &content) {
    auto list = content.is_array() ? content : nlohmann::json::array();
    std::vector<Model::SpocSchedule> records;
    for (const auto &item : list) {
        if (!item.is_object()) continue;
        Model::SpocSchedule record;
        record.id = json_string(item, "id");
        record.course_id = json_string(item, "kcid");
        record.course_name = json_string(item, "kcmc");
        record.teacher = json_string(item, "jsxm");
        record.weekday = json_string(item, "weekday");
        record.classroom = json_string(item, "skdd");
        record.time_text = json_string(item, "kcsj");
        auto [start, end] = parse_spoc_time_range(record.time_text);
        record.start_time = std::move(start);
        record.end_time = std::move(end);
        if (record.id.empty()) record.id = record.course_id.empty() ? record.course_name + ":" + record.time_text : record.course_id + ":" + record.time_text;
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::SpocCourse> parse_spoc_courses(const nlohmann::json &content) {
    auto list = content.is_array() ? content : nlohmann::json::array();
    std::vector<Model::SpocCourse> records;
    for (const auto &item : list) {
        if (!item.is_object()) continue;
        Model::SpocCourse record;
        record.id = json_string(item, "kcid");
        record.name = json_string(item, "kcmc");
        record.teacher = json_string(item, "skjs");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

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
