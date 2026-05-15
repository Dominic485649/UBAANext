/**
 * @file OutputFormatter.cpp
 * @brief 输出格式化器实现
 */

#include "OutputFormatter.hpp"

#include "Console.hpp"

#include <nlohmann/json.hpp>

namespace UBAANextCli {

using json = nlohmann::json;

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

    if (week > 0) {
        Console::println("第 {} 周课程:", week);
    } else {
        Console::println("今日课程:");
    }
    for (size_t i = 0; i < courses.size(); ++i) {
        const auto &c = courses[i];
        Console::println("{}. {} | {}-{}节 | {}", i + 1, c.name,
                     c.section_start, c.section_end, c.classroom);
    }
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

    Console::println("考试:");
    for (size_t i = 0; i < exams.size(); ++i) {
        const auto &e = exams[i];
        Console::println("{}. {} | {} | {} | 座位: {}", i + 1,
                     e.course_name, e.location, e.time_text, e.seat_no);
    }
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

    Console::println("成绩:");
    for (size_t i = 0; i < grades.size(); ++i) {
        const auto &g = grades[i];
        Console::println("{}. {} | {} | {} 学分 | {}", i + 1, g.course_name, g.score, g.credit, g.term_code);
    }
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

    Console::println("空闲教室:");
    for (const auto &[building, rooms] : qr.buildings) {
        Console::println("  {}:", building);
        for (const auto &room : rooms) {
            Console::print("    {} - 空闲节次: ", room.name);
            for (size_t i = 0; i < room.free_sections.size(); ++i) {
                if (i > 0) Console::print(", ");
                Console::print("{}", room.free_sections[i]);
            }
            Console::println();
        }
    }
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

    Console::println("学期列表:");
    for (size_t i = 0; i < terms.size(); ++i) {
        const auto &t = terms[i];
        Console::println("{}. {} {} [{}]", i + 1, t.name, t.code,
                     t.selected ? "当前" : "");
    }
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

    Console::println("教学周列表:");
    for (size_t i = 0; i < weeks.size(); ++i) {
        const auto &w = weeks[i];
        Console::println("{}. {} ({} - {}){}", i + 1, w.name,
                     w.start_date, w.end_date,
                     w.is_current ? " [当前周]" : "");
    }
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

    Console::println("用户: {}", account.display_name);
    Console::println("学号: {}", account.student_id);
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

    Console::println("{}", message);
    Console::println("用户: {}", account.display_name);
    Console::println("学号: {}", account.student_id);
}

namespace {

json record_to_json(const UBAANext::Model::FeatureRecord &record) {
    return {
        {"id", record.id},
        {"title", record.title},
        {"status", record.status},
        {"fields", record.fields},
    };
}

} // namespace

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

    Console::println("{}:", key);
    for (size_t i = 0; i < records.size(); ++i) {
        const auto &record = records[i];
        Console::println("{}. {} [{}] {}", i + 1, record.title, record.status, record.id);
    }
}

void OutputFormatter::print_record(const std::string &key, const UBAANext::Model::FeatureRecord &record) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{key, record_to_json(record)}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("{}: {} [{}]", key, record.title, record.status);
    Console::println("ID: {}", record.id);
    for (const auto &[name, value] : record.fields) {
        Console::println("{}: {}", name, value);
    }
}

void OutputFormatter::print_mutation(const UBAANext::Model::MutationResult &result) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"accepted", result.accepted}, {"message", result.message}, {"result", record_to_json(result.summary)}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("{}", result.message);
    Console::println("对象: {} [{}]", result.summary.title, result.summary.id);
}

void OutputFormatter::print_version(const std::string &version) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"version", version}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("UBAA Next {}", version);
}

void OutputFormatter::print_error(const UBAANext::Error &error) {
    if (m_json) {
        json err = {{"code", std::string(error_code_to_string(error.code))}, {"message", error.message}};
        json out = {{"ok", false}, {"data", nullptr}, {"error", err}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::eprintln("错误: {}", error.message);
}

void OutputFormatter::print_message(const std::string &msg) {
    if (m_json) {
        json out = {{"ok", true}, {"data", {{"message", msg}}}, {"error", nullptr}};
        Console::println("{}", out.dump(2));
        return;
    }

    Console::println("{}", msg);
}

} // namespace UBAANextCli
