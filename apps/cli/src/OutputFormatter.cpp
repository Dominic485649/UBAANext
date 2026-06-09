/**
 * @file OutputFormatter.cpp
 * @brief 输出格式化器实现
 */

#include "OutputFormatter.hpp"

#include "Console.hpp"
#include "SecurityRedaction.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace UBAANextCli {

using json = nlohmann::json;

namespace {

struct TableColumn {
    std::string name;
    bool right_align = false;
    std::size_t max_width = 0;
};

using TableRow = std::vector<std::string>;

enum class TextStyle {
    Plain,
    Title,
    Header,
    Success,
    Warning,
    Error,
    Muted,
    Identifier,
    Highlight,
    Important,
};

bool is_stdout_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

std::optional<std::string> environment_value(const char *name) {
#if defined(_WIN32)
    char *value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) return std::nullopt;
    std::string result(value, size > 0 ? size - 1 : 0);
    std::free(value);
    return result;
#else
    const auto *value = std::getenv(name);
    if (value == nullptr) return std::nullopt;
    return std::string(value);
#endif
}

bool color_enabled() {
    const auto no_color = environment_value("NO_COLOR");
    if (no_color && !no_color->empty()) return false;
    const auto ubaa_no_color = environment_value("UBAANEXT_NO_COLOR");
    if (ubaa_no_color && !ubaa_no_color->empty() && *ubaa_no_color != "0") return false;
    const auto force_color = environment_value("UBAANEXT_FORCE_COLOR");
    if (force_color && !force_color->empty() && *force_color != "0") return true;
    const auto ci = environment_value("CI");
    if (ci && !ci->empty()) return false;
    return is_stdout_terminal();
}

std::string style_code(TextStyle style) {
    switch (style) {
    case TextStyle::Title: return "\033[1;36m";
    case TextStyle::Header: return "\033[1m";
    case TextStyle::Success: return "\033[32m";
    case TextStyle::Warning: return "\033[33m";
    case TextStyle::Error: return "\033[31m";
    case TextStyle::Muted: return "\033[90m";
    case TextStyle::Identifier: return "\033[1;36m";
    case TextStyle::Highlight: return "\033[1;35m";
    case TextStyle::Important: return "\033[1;33m";
    case TextStyle::Plain: break;
    }
    return {};
}

std::string colorize(std::string text, TextStyle style) {
    if (style == TextStyle::Plain || !color_enabled()) return text;
    return style_code(style) + text + "\033[0m";
}

std::string success_text(std::string text) {
    return colorize(std::move(text), TextStyle::Success);
}

std::string muted_text(std::string text) {
    return colorize(std::move(text), TextStyle::Muted);
}

std::size_t utf8_sequence_length(std::string_view text, std::size_t offset) {
    const auto c = static_cast<unsigned char>(text[offset]);
    const auto remaining = text.size() - offset;
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0 && remaining >= 2) return 2;
    if ((c & 0xF0) == 0xE0 && remaining >= 3) return 3;
    if ((c & 0xF8) == 0xF0 && remaining >= 4) return 4;
    return 1;
}

std::uint32_t decode_codepoint(std::string_view text, std::size_t offset, std::size_t length) {
    const auto c0 = static_cast<unsigned char>(text[offset]);
    if (length == 1) return c0;
    if (length == 2) return ((c0 & 0x1F) << 6) | (static_cast<unsigned char>(text[offset + 1]) & 0x3F);
    if (length == 3) {
        return ((c0 & 0x0F) << 12) |
               ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(text[offset + 2]) & 0x3F);
    }
    return ((c0 & 0x07) << 18) |
           ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(text[offset + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(text[offset + 3]) & 0x3F);
}

bool is_combining_codepoint(std::uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}

bool is_wide_codepoint(std::uint32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||
           (cp >= 0x2329 && cp <= 0x232A) ||
           (cp >= 0x2E80 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE10 && cp <= 0xFE19) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x1F300 && cp <= 0x1FAFF);
}

std::size_t codepoint_display_width(std::uint32_t cp) {
    if (cp == '\t') return 4;
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (is_combining_codepoint(cp)) return 0;
    return is_wide_codepoint(cp) ? 2 : 1;
}

std::size_t display_width(std::string_view text) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        if (text[i] == '\033') {
            const auto end = text.find('m', i + 1);
            if (end != std::string_view::npos) {
                i = end + 1;
                continue;
            }
        }
        const auto length = utf8_sequence_length(text, i);
        width += codepoint_display_width(decode_codepoint(text, i, length));
        i += length;
    }
    return width;
}

std::string truncate_display(std::string text, std::size_t max_width) {
    if (max_width == 0 || display_width(text) <= max_width) return text;
    const std::string suffix = max_width >= 3 ? "..." : std::string(max_width, '.');
    const auto suffix_width = display_width(suffix);
    const auto body_width = max_width > suffix_width ? max_width - suffix_width : 0;
    std::string out;
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto length = utf8_sequence_length(text, i);
        const auto cp_width = codepoint_display_width(decode_codepoint(text, i, length));
        if (width + cp_width > body_width) break;
        out.append(text.substr(i, length));
        width += cp_width;
        i += length;
    }
    out += suffix;
    return out;
}

std::string clean_cell(std::string text, std::size_t max_width = 0) {
    text = redact_sensitive_text(text);
    for (auto &ch : text) {
        if (ch == '\r' || ch == '\n' || ch == '\t') ch = ' ';
    }
    if (text.empty()) text = "-";
    return truncate_display(std::move(text), max_width);
}

std::string pad_cell(const std::string &text, std::size_t width, bool right_align) {
    const auto cell_width = display_width(text);
    if (cell_width >= width) return text;
    const std::string padding(width - cell_width, ' ');
    return right_align ? padding + text : text + padding;
}

std::string render_row(const std::vector<std::string> &cells,
                       const std::vector<std::size_t> &widths,
                       const std::vector<TableColumn> &columns) {
    std::string line;
    for (std::size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) line += "  ";
        const auto cell = i < cells.size() ? cells[i] : std::string{};
        line += pad_cell(cell, widths[i], columns[i].right_align);
    }
    return line;
}

std::string lowercase_ascii(std::string text);

bool contains_ansi(std::string_view text) {
    return text.find("\033[") != std::string_view::npos;
}

bool contains_any(std::string_view text, const std::vector<std::string_view> &needles) {
    for (auto needle : needles) {
        if (text.find(needle) != std::string_view::npos) return true;
    }
    return false;
}

TextStyle style_for_cell(const std::string &label, const std::string &value) {
    if (value.empty() || value == "-") return TextStyle::Muted;
    const auto normalized_label = lowercase_ascii(label);
    const auto normalized_value = lowercase_ascii(value);

    if (contains_any(normalized_value, {"error", "failed", "failure", "invalid", "expired", "denied", "rejected"}) ||
        contains_any(value, {"错误", "失败", "过期", "拒绝", "不可用", "无效"})) {
        return TextStyle::Error;
    }
    if (contains_any(normalized_value, {"warning", "pending", "waiting", "running", "fallback", "plaintext", "confirm"}) ||
        contains_any(value, {"警告", "待", "确认", "明文", "风险", "fallback"})) {
        return TextStyle::Warning;
    }
    if (normalized_value == "true" || normalized_value == "yes" || normalized_value == "current" || normalized_value == "ok" ||
        normalized_value == "success" || normalized_value == "completed" || normalized_value == "saved" ||
        contains_any(value, {"成功", "已保存", "已登录", "完成"})) {
        return TextStyle::Success;
    }
    if (normalized_value == "false" || normalized_value == "no") return TextStyle::Warning;

    if (contains_any(normalized_label, {"id", "code", "student", "serial", "floor", "seat", "booking", "order", "token"})) {
        return TextStyle::Identifier;
    }
    if (contains_any(normalized_label, {"name", "title", "course", "display", "teacher", "classroom", "room", "location", "capability"})) {
        return TextStyle::Highlight;
    }
    if (contains_any(normalized_label, {"time", "date", "section", "score", "credit", "gpa", "status", "selected", "current", "enabled"})) {
        return TextStyle::Important;
    }
    return TextStyle::Plain;
}

std::string emphasize_cell(const std::string &column, const std::string &value, const TableRow &cleaned_row, std::size_t column_index) {
    if (contains_ansi(value)) return value;
    std::string label = column;
    if ((column == "Value" || column == "Enabled") && !cleaned_row.empty()) {
        if (cleaned_row.size() >= 2 && column_index == 1) label = cleaned_row[0];
        if (cleaned_row.size() >= 3 && column_index == 2) label = cleaned_row[1];
    }
    return colorize(value, style_for_cell(label, value));
}

void render_table(const std::string &title,
                  const std::vector<TableColumn> &columns,
                  const std::vector<TableRow> &rows) {
    if (!title.empty()) {
        Console::println("{}", colorize(title, TextStyle::Title));
        Console::println();
    }

    std::vector<TableRow> normalized_rows;
    normalized_rows.reserve(rows.size());
    std::vector<std::size_t> widths;
    widths.reserve(columns.size());
    for (const auto &column : columns) widths.push_back(display_width(column.name));

    for (const auto &row : rows) {
        TableRow cleaned;
        cleaned.reserve(columns.size());
        for (std::size_t i = 0; i < columns.size(); ++i) {
            const auto value = i < row.size() ? row[i] : std::string{};
            cleaned.push_back(clean_cell(value, columns[i].max_width));
        }

        TableRow normalized;
        normalized.reserve(columns.size());
        for (std::size_t i = 0; i < columns.size(); ++i) {
            normalized.push_back(emphasize_cell(columns[i].name, cleaned[i], cleaned, i));
            widths[i] = std::max(widths[i], display_width(normalized.back()));
        }
        normalized_rows.push_back(std::move(normalized));
    }

    TableRow headers;
    headers.reserve(columns.size());
    for (const auto &column : columns) headers.push_back(colorize(column.name, TextStyle::Header));
    Console::println("{}", render_row(headers, widths, columns));

    TableRow separators;
    separators.reserve(columns.size());
    for (const auto width : widths) separators.push_back(muted_text(std::string(width, '-')));
    Console::println("{}", render_row(separators, widths, columns));

    for (const auto &row : normalized_rows) {
        Console::println("{}", render_row(row, widths, columns));
    }
    if (normalized_rows.empty()) {
        Console::println("(无记录)");
    }
}

std::string value_or_dash(const std::string &value) {
    return value.empty() ? "-" : value;
}

std::string bool_text(bool value) {
    return value ? success_text("true") : colorize("false", TextStyle::Warning);
}

std::string range_text(int start, int end) {
    if (start <= 0 && end <= 0) return "-";
    if (end <= 0 || start == end) return std::to_string(start);
    return std::to_string(start) + "-" + std::to_string(end);
}

std::string time_range_text(const std::string &start, const std::string &end) {
    if (start.empty() && end.empty()) return "-";
    if (start.empty()) return end;
    if (end.empty()) return start;
    return start + "-" + end;
}

std::string weekday_text(int day) {
    switch (day) {
    case 1: return "周一";
    case 2: return "周二";
    case 3: return "周三";
    case 4: return "周四";
    case 5: return "周五";
    case 6: return "周六";
    case 7: return "周日";
    default: return day > 0 ? std::to_string(day) : "-";
    }
}

std::string join_ints(const std::vector<int> &values) {
    if (values.empty()) return "-";
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out += ",";
        out += std::to_string(values[i]);
    }
    return out;
}

std::string exam_status_text(UBAANext::Model::ExamStatus status) {
    switch (status) {
    case UBAANext::Model::ExamStatus::Pending: return "pending";
    case UBAANext::Model::ExamStatus::Arranged: return "arranged";
    case UBAANext::Model::ExamStatus::Finished: return "finished";
    }
    return "unknown";
}

std::string title_for_key(const std::string &key) {
    static const std::map<std::string, std::string> titles = {
        {"account", "Account"},
        {"areas", "Areas"},
        {"area", "Area"},
        {"assignment", "Assignment"},
        {"assignments", "Assignments"},
        {"booking", "Booking"},
        {"bookings", "Bookings"},
        {"course", "Course"},
        {"courses", "Courses"},
        {"details", "Details"},
        {"evaluation", "Evaluation"},
        {"evaluations", "Evaluations"},
        {"libraries", "Libraries"},
        {"library", "Library"},
        {"orders", "Orders"},
        {"order", "Order"},
        {"profile", "Profile"},
        {"records", "Records"},
        {"reservation", "Reservation"},
        {"reservations", "Reservations"},
        {"seat", "Seat"},
        {"seats", "Seats"},
        {"signin", "Sign-in"},
        {"signins", "Sign-ins"},
        {"sites", "Sites"},
        {"site", "Site"},
        {"stats", "Stats"},
        {"todo", "Todo"},
        {"todos", "Todos"},
        {"user", "User"},
    };
    const auto it = titles.find(key);
    if (it != titles.end()) return it->second;

    std::string title = key.empty() ? "Records" : key;
    bool capitalize_next = true;
    for (auto &ch : title) {
        if (ch == '-' || ch == '_') {
            ch = ' ';
            capitalize_next = true;
            continue;
        }
        if (capitalize_next) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            capitalize_next = false;
        }
    }
    return title;
}

std::string lowercase_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string status_badge(const std::string &status) {
    if (status.empty()) return "-";
    const auto normalized = lowercase_ascii(status);
    if (normalized.find("error") != std::string::npos || normalized.find("fail") != std::string::npos ||
        normalized == "rejected" || normalized == "denied") {
        return colorize("✗ " + status, TextStyle::Error);
    }
    if (normalized == "pending" || normalized == "waiting" || normalized == "running") {
        return colorize("… " + status, TextStyle::Warning);
    }
    if (normalized == "skipped" || normalized == "unchanged") {
        return muted_text("- " + status);
    }
    if (normalized == "success" || normalized == "ok" || normalized == "completed" || normalized == "initialized" ||
        normalized == "stored" || normalized == "updated" || normalized == "accepted" || normalized == "saved" ||
        normalized == "deleted") {
        return success_text("✓ " + status);
    }
    return status;
}

std::string field_label(const std::string &name) {
    static const std::map<std::string, std::string> labels = {
        {"accepted", "Accepted"},
        {"cache", "Cache"},
        {"campus", "Campus"},
        {"cardId", "Card ID"},
        {"changed", "Changed"},
        {"completedRounds", "Completed Rounds"},
        {"configPath", "Config Path"},
        {"date", "Date"},
        {"entranceImage", "Entrance Image"},
        {"entranceMachine", "Entrance Machine"},
        {"exitImage", "Exit Image"},
        {"exitMachine", "Exit Machine"},
        {"failure", "Failure"},
        {"imagesDir", "Images Dir"},
        {"inWindow", "In Window"},
        {"lastError", "Last Error"},
        {"lastMessage", "Last Message"},
        {"logsDir", "Logs Dir"},
        {"message", "Message"},
        {"nextAction", "Next Action"},
        {"nextRunAt", "Next Run At"},
        {"now", "Now"},
        {"path", "Path"},
        {"pollSeconds", "Poll Seconds"},
        {"remainingSeconds", "Remaining Seconds"},
        {"remoteRequest", "Remote Request"},
        {"root", "Root"},
        {"rounds", "Rounds"},
        {"settingsPath", "Settings Path"},
        {"skipped", "Skipped"},
        {"statePath", "State Path"},
        {"studentId", "Student ID"},
        {"success", "Success"},
        {"termCount", "Term Count"},
        {"total", "Total"},
        {"usersPath", "Users Path"},
        {"waitMinutes", "Wait Minutes"},
    };
    const auto it = labels.find(name);
    return it != labels.end() ? it->second : name;
}

int field_priority(const std::string &name) {
    static const std::vector<std::string> priority = {
        "message", "lastMessage", "lastError", "studentId", "date", "now", "inWindow", "remoteRequest",
        "total", "success", "failure", "skipped", "completedRounds", "termCount", "nextAction", "nextRunAt",
        "remainingSeconds", "waitMinutes",
    };
    const auto it = std::find(priority.begin(), priority.end(), name);
    if (it == priority.end()) return 1000;
    return static_cast<int>(std::distance(priority.begin(), it));
}

std::vector<std::pair<std::string, std::string>> sorted_fields(const std::map<std::string, std::string> &fields,
                                                               bool include_empty) {
    std::vector<std::pair<std::string, std::string>> values;
    for (const auto &[name, value] : fields) {
        if (!include_empty && value.empty()) continue;
        values.push_back({name, value});
    }
    std::sort(values.begin(), values.end(), [](const auto &lhs, const auto &rhs) {
        const auto left_priority = field_priority(lhs.first);
        const auto right_priority = field_priority(rhs.first);
        if (left_priority != right_priority) return left_priority < right_priority;
        return lhs.first < rhs.first;
    });
    return values;
}

json record_to_json(const UBAANext::Model::FeatureRecord &record) {
    return {
        {"id", record.id},
        {"title", record.title},
        {"status", record.status},
        {"fields", record.fields},
    };
}

} // namespace

OutputFormatter::OutputFormatter(bool json_mode) : m_json(json_mode) {}

void OutputFormatter::print_courses(const std::vector<UBAANext::Model::Course> &courses, int week) {
    if (m_json) {
        json arr = json::array();
        for (const auto &c : courses) {
            arr.push_back({
                {"name",        c.name},
                {"teacher",     c.teacher},
                {"classroom",   c.classroom},
                {"weekStart",   c.week_start},
                {"weekEnd",     c.week_end},
                {"dayOfWeek",   c.day_of_week},
                {"sectionStart", c.section_start},
                {"sectionEnd",  c.section_end},
                {"courseCode",  c.course_code},
                {"credit",      c.credit},
                {"beginTime",   c.begin_time},
                {"endTime",     c.end_time},
            });
        }
        json data = {{"courses", arr}};
        if (week > 0) {
            data["week"] = week;
        }
        json out = {{"ok", true}, {"data", data}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(courses.size());
    for (std::size_t i = 0; i < courses.size(); ++i) {
        const auto &c = courses[i];
        rows.push_back({
            std::to_string(i + 1),
            weekday_text(c.day_of_week),
            range_text(c.section_start, c.section_end),
            time_range_text(c.begin_time, c.end_time),
            c.name,
            value_or_dash(c.teacher),
            value_or_dash(c.classroom),
            value_or_dash(c.course_code),
            value_or_dash(c.credit),
            value_or_dash(c.id),
        });
    }
    render_table(week > 0 ? "Courses - Week " + std::to_string(week) : "Courses - Today",
                 {{"Index", true}, {"Day"}, {"Sections"}, {"Time"}, {"Name"}, {"Teacher"}, {"Classroom"}, {"Code"}, {"Credit"}, {"Id"}},
                 rows);
}

void OutputFormatter::print_exams(const std::vector<UBAANext::Model::Exam> &exams) {
    if (m_json) {
        json arr = json::array();
        for (const auto &e : exams) {
            arr.push_back({
                {"courseName", e.course_name},
                {"location",   e.location},
                {"timeText",   e.time_text},
                {"courseNo",   e.course_no},
                {"examDate",   e.exam_date},
                {"startTime",  e.start_time},
                {"endTime",    e.end_time},
                {"seatNo",     e.seat_no},
                {"examType",   e.exam_type},
                {"status",     e.status},
            });
        }
        json out = {{"ok", true}, {"data", {{"exams", arr}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(exams.size());
    for (std::size_t i = 0; i < exams.size(); ++i) {
        const auto &e = exams[i];
        rows.push_back({
            std::to_string(i + 1),
            e.course_name,
            value_or_dash(e.exam_type),
            value_or_dash(e.time_text.empty() ? time_range_text(e.start_time, e.end_time) : e.time_text),
            value_or_dash(e.location),
            value_or_dash(e.seat_no),
            exam_status_text(e.status),
            value_or_dash(e.course_no),
            value_or_dash(e.id),
        });
    }
    render_table("Exams",
                 {{"Index", true}, {"Course"}, {"Type"}, {"Time"}, {"Location"}, {"Seat"}, {"Status"}, {"CourseNo"}, {"Id"}},
                 rows);
}

void OutputFormatter::print_grades(const std::vector<UBAANext::Model::Grade> &grades) {
    if (m_json) {
        json arr = json::array();
        for (const auto &g : grades) {
            arr.push_back({
                {"id", g.id},
                {"courseName", g.course_name},
                {"courseCode", g.course_code},
                {"courseType", g.course_type},
                {"credit", g.credit},
                {"score", g.score},
                {"gradePoint", g.grade_point},
                {"termCode", g.term_code},
                {"status", g.raw_status},
            });
        }
        json out = {{"ok", true}, {"data", {{"grades", arr}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(grades.size());
    for (std::size_t i = 0; i < grades.size(); ++i) {
        const auto &g = grades[i];
        rows.push_back({
            std::to_string(i + 1),
            g.course_name,
            value_or_dash(g.score),
            value_or_dash(g.credit),
            value_or_dash(g.grade_point),
            value_or_dash(g.course_type),
            value_or_dash(g.term_code),
            value_or_dash(g.course_code),
            value_or_dash(g.raw_status),
            value_or_dash(g.id),
        });
    }
    render_table("Grades",
                 {{"Index", true}, {"Course"}, {"Score"}, {"Credit"}, {"GPA"}, {"Type"}, {"Term"}, {"Code"}, {"Status"}, {"Id"}},
                 rows);
}

void OutputFormatter::print_classrooms(const UBAANext::Model::ClassroomQueryResult &qr) {
    if (m_json) {
        json buildings = json::object();
        for (const auto &[building, rooms] : qr.buildings) {
            json arr = json::array();
            for (const auto &room : rooms) {
                arr.push_back({
                    {"name",          room.name},
                    {"floorId",       room.floor_id},
                    {"freeSections",  room.free_sections},
                });
            }
            buildings[building] = arr;
        }
        json out = {{"ok", true}, {"data", {{"buildings", buildings}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    std::size_t index = 1;
    for (const auto &[building, rooms] : qr.buildings) {
        for (const auto &room : rooms) {
            rows.push_back({
                std::to_string(index++),
                building,
                room.name,
                value_or_dash(room.floor_id),
                join_ints(room.free_sections),
                value_or_dash(room.id),
            });
        }
    }
    render_table("Classrooms",
                 {{"Index", true}, {"Building"}, {"Room"}, {"FloorId"}, {"FreeSections"}, {"Id"}},
                 rows);
}

void OutputFormatter::print_terms(const std::vector<UBAANext::Model::Term> &terms) {
    if (m_json) {
        json arr = json::array();
        for (const auto &t : terms) {
            arr.push_back({
                {"code",     t.code},
                {"name",     t.name},
                {"selected", t.selected},
                {"index",    t.index},
            });
        }
        json out = {{"ok", true}, {"data", {{"terms", arr}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(terms.size());
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const auto &t = terms[i];
        rows.push_back({
            std::to_string(i + 1),
            std::to_string(t.index),
            t.name,
            t.code,
            t.selected ? "current" : "-",
        });
    }
    render_table("Terms",
                 {{"Index", true}, {"TermIndex", true}, {"Name"}, {"Code"}, {"Selected"}},
                 rows);
}

void OutputFormatter::print_weeks(const std::vector<UBAANext::Model::Week> &weeks) {
    if (m_json) {
        json arr = json::array();
        for (const auto &w : weeks) {
            arr.push_back({
                {"serialNumber", w.serial_number},
                {"name",         w.name},
                {"startDate",    w.start_date},
                {"endDate",      w.end_date},
                {"isCurrent",    w.is_current},
            });
        }
        json out = {{"ok", true}, {"data", {{"weeks", arr}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(weeks.size());
    for (std::size_t i = 0; i < weeks.size(); ++i) {
        const auto &w = weeks[i];
        rows.push_back({
            std::to_string(i + 1),
            std::to_string(w.serial_number),
            w.name,
            value_or_dash(w.start_date),
            value_or_dash(w.end_date),
            w.is_current ? "current" : "-",
        });
    }
    render_table("Weeks",
                 {{"Index", true}, {"Serial", true}, {"Name"}, {"StartDate"}, {"EndDate"}, {"Current"}},
                 rows);
}

void OutputFormatter::print_account(const UBAANext::Model::Account &account) {
    if (m_json) {
        json data = {
            {"studentId",   account.student_id},
            {"displayName", account.display_name},
        };
        json out = {{"ok", true}, {"data", data}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    render_table("Account",
                 {{"Field"}, {"Value"}},
                 {{"DisplayName", account.display_name}, {"StudentId", account.student_id}});
}

void OutputFormatter::print_login_result(const std::string &message, const UBAANext::Model::Account &account) {
    if (m_json) {
        json data = {
            {"message", message},
            {"account", {
                {"studentId", account.student_id},
                {"displayName", account.display_name},
            }},
        };
        json out = {{"ok", true}, {"data", data}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    render_table("Login",
                 {{"Field"}, {"Value"}},
                 {{"Message", message}, {"DisplayName", account.display_name}, {"StudentId", account.student_id}});
}

void OutputFormatter::print_records(const std::string &key, const std::vector<UBAANext::Model::FeatureRecord> &records) {
    if (m_json) {
        json arr = json::array();
        for (const auto &record : records) {
            arr.push_back(record_to_json(record));
        }
        json out = {{"ok", true}, {"data", {{key, arr}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows;
    rows.reserve(records.size());
    std::vector<TableRow> detail_rows;
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto &record = records[i];
        const auto index = std::to_string(i + 1);
        rows.push_back({
            index,
            record.title,
            status_badge(record.status),
            value_or_dash(record.id),
        });
        for (const auto &[name, value] : sorted_fields(record.fields, false)) {
            detail_rows.push_back({index, field_label(name), value});
        }
    }
    render_table(title_for_key(key),
                 {{"Index", true}, {"Title", false, 48}, {"Status"}, {"Id", false, 28}},
                 rows);
    if (!detail_rows.empty()) {
        Console::println();
        render_table(title_for_key(key) + " Details",
                     {{"Record", true}, {"Field"}, {"Value", false, 120}},
                     detail_rows);
    }
}

void OutputFormatter::print_record(const std::string &key, const UBAANext::Model::FeatureRecord &record) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{key, record_to_json(record)}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    std::vector<TableRow> rows = {
        {"Title", record.title},
        {"Status", status_badge(record.status)},
        {"Id", value_or_dash(record.id)},
    };
    for (const auto &[name, value] : sorted_fields(record.fields, true)) {
        rows.push_back({field_label(name), value});
    }
    render_table(title_for_key(key), {{"Field"}, {"Value", false, 120}}, rows);
}

void OutputFormatter::print_mutation(const UBAANext::Model::MutationResult &result) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"accepted", result.accepted}, {"message", result.message}, {"result", record_to_json(result.summary)}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    render_table("Mutation",
                 {{"Field"}, {"Value", false, 120}},
                 {{"Accepted", bool_text(result.accepted)}, {"Message", result.message}});
    Console::println();
    render_table("Result",
                 {{"Title"}, {"Status"}, {"Id"}},
                 {{result.summary.title, status_badge(result.summary.status), value_or_dash(result.summary.id)}});
    const auto detail_fields = sorted_fields(result.summary.fields, false);
    if (!detail_fields.empty()) {
        std::vector<TableRow> rows;
        rows.reserve(detail_fields.size());
        for (const auto &[name, value] : detail_fields) rows.push_back({field_label(name), value});
        Console::println();
        render_table("Result Details", {{"Field"}, {"Value", false, 120}}, rows);
    }
}

void OutputFormatter::print_capabilities(const UBAANext::PlatformCapabilities &capabilities) {
    if (m_json) {
        json data = {
            {"capabilities", {
                {"realNetwork", capabilities.real_network},
                {"secureCookiePersistence", capabilities.secure_cookie_persistence},
                {"cookiePersistence", capabilities.cookie_persistence},
                {"redirectControl", capabilities.redirect_control},
                {"protocolCrypto", capabilities.protocol_crypto},
                {"secureStore", capabilities.secure_store},
                {"appDataPath", capabilities.app_data_path},
                {"uploadBytes", capabilities.upload_bytes},
                {"liveLogin", capabilities.live_login},
                {"writeOperations", capabilities.write_operations},
                {"desktopGui", capabilities.desktop_gui},
                {"winfspMount", capabilities.mount_windows_drive},
                {"cloudFilesMount", capabilities.mount_windows_sync},
                {"fuseMount", capabilities.mount_linux_userspace},
            }}
        };
        json out = {{"ok", true}, {"data", data}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    render_table("Capabilities",
                 {{"Capability"}, {"Enabled"}},
                 {
                     {"realNetwork", bool_text(capabilities.real_network)},
                     {"secureCookiePersistence", bool_text(capabilities.secure_cookie_persistence)},
                     {"cookiePersistence", bool_text(capabilities.cookie_persistence)},
                     {"redirectControl", bool_text(capabilities.redirect_control)},
                     {"protocolCrypto", bool_text(capabilities.protocol_crypto)},
                     {"secureStore", bool_text(capabilities.secure_store)},
                     {"appDataPath", bool_text(capabilities.app_data_path)},
                     {"uploadBytes", bool_text(capabilities.upload_bytes)},
                     {"liveLogin", bool_text(capabilities.live_login)},
                     {"writeOperations", bool_text(capabilities.write_operations)},
                     {"desktopGui", bool_text(capabilities.desktop_gui)},
                     {"winfspMount", bool_text(capabilities.mount_windows_drive)},
                     {"cloudFilesMount", bool_text(capabilities.mount_windows_sync)},
                     {"fuseMount", bool_text(capabilities.mount_linux_userspace)},
                 });
}

void OutputFormatter::print_version(const std::string &version) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"version", version}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("{}", success_text("UBAA Next " + version));
}

void OutputFormatter::print_error(const UBAANext::Error &error) {
    const auto message = redact_sensitive_text(error.message);
    if (m_json) {
        json err = {{"code", std::string(error_code_to_string(error.code))}, {"message", message}};
        json out = {{"ok", false}, {"data", nullptr}, {"error", err}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::eprintln("{}", colorize("错误: " + message, TextStyle::Error));
}

void OutputFormatter::print_message(const std::string &msg) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"message", msg}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("{}", success_text(msg));
}

void OutputFormatter::print_fields(const std::string &title, const std::vector<std::pair<std::string, std::string>> &fields) {
    std::vector<TableRow> rows;
    rows.reserve(fields.size());
    for (const auto &[name, value] : fields) {
        rows.push_back({name, value});
    }
    render_table(title, {{"Field"}, {"Value", false, 120}}, rows);
}

} // namespace UBAANextCli
