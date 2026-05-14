/**
 * @file JsonParser.cpp
 * @brief JSON 解析器实现
 *
 * 使用 nlohmann/json 将 API JSON 响应解析为 Model 结构体。
 * 缺失字段使用默认值（容错解析），JSON 语法错误返回 ParseError。
 */

#include <UBAANext/Parser/JsonParser.hpp>

#include <nlohmann/json.hpp>

namespace UBAANext::Parser {

Result<std::vector<Model::Course>> parse_courses(const std::string &json) {
    try {
        auto j = nlohmann::json::parse(json);
        if (!j.is_array()) {
            return make_error(ErrorCode::ParseError, "课程数据必须是 JSON 数组");
        }
        std::vector<Model::Course> courses;
        courses.reserve(j.size());
        for (const auto &item : j) {
            Model::Course c;
            c.id            = item.value("id", "");
            c.name          = item.value("name", "");
            c.teacher       = item.value("teacher", "");
            c.classroom     = item.value("classroom", "");
            c.week_start    = item.value("weekStart", 0);
            c.week_end      = item.value("weekEnd", 0);
            c.day_of_week   = item.value("dayOfWeek", 0);
            c.section_start = item.value("sectionStart", 0);
            c.section_end   = item.value("sectionEnd", 0);
            c.course_code   = item.value("courseCode", "");
            c.credit        = item.value("credit", "");
            c.begin_time    = item.value("beginTime", "");
            c.end_time      = item.value("endTime", "");
            courses.push_back(std::move(c));
        }
        return courses;
    } catch (const nlohmann::json::exception &e) {
        return make_error(ErrorCode::ParseError,
                          std::string("JSON 解析失败: ") + e.what());
    }
}

Result<std::vector<Model::Exam>> parse_exams(const std::string &json) {
    try {
        auto j = nlohmann::json::parse(json);
        if (!j.is_array()) {
            return make_error(ErrorCode::ParseError, "考试数据必须是 JSON 数组");
        }
        std::vector<Model::Exam> exams;
        exams.reserve(j.size());
        for (const auto &item : j) {
            Model::Exam e;
            e.id          = item.value("id", "");
            e.course_name = item.value("courseName", "");
            e.location    = item.value("location", "");
            e.time_text   = item.value("timeText", "");
            e.course_no   = item.value("courseNo", "");
            e.exam_date   = item.value("examDate", "");
            e.start_time  = item.value("startTime", "");
            e.end_time    = item.value("endTime", "");
            e.seat_no     = item.value("seatNo", "");
            e.exam_type   = item.value("examType", "");

            int status_val = item.value("status", 0);
            if (status_val >= 0 && status_val <= 2) {
                e.status = static_cast<Model::ExamStatus>(status_val);
            } else {
                e.status = Model::ExamStatus::Pending;
            }

            exams.push_back(std::move(e));
        }
        return exams;
    } catch (const nlohmann::json::exception &e) {
        return make_error(ErrorCode::ParseError,
                          std::string("JSON 解析失败: ") + e.what());
    }
}

Result<Model::ClassroomQueryResult> parse_classrooms(const std::string &json) {
    try {
        auto j = nlohmann::json::parse(json);
        if (!j.is_object()) {
            return make_error(ErrorCode::ParseError, "教室数据必须是 JSON 对象");
        }

        Model::ClassroomQueryResult result;

        auto buildings_it = j.find("buildings");
        if (buildings_it == j.end() || !buildings_it->is_object()) {
            return result;
        }

        for (auto &[building_name, rooms] : buildings_it->items()) {
            if (!rooms.is_array()) continue;

            std::vector<Model::ClassroomInfo> room_list;
            room_list.reserve(rooms.size());

            for (const auto &room : rooms) {
                Model::ClassroomInfo info;
                info.id       = room.value("id", "");
                info.name     = room.value("name", "");
                info.floor_id = room.value("floorId", "");

                auto sections_it = room.find("freeSections");
                if (sections_it != room.end() && sections_it->is_array()) {
                    for (const auto &sec : *sections_it) {
                        if (sec.is_number_integer()) {
                            info.free_sections.push_back(sec.get<int>());
                        }
                    }
                }

                room_list.push_back(std::move(info));
            }

            result.buildings[building_name] = std::move(room_list);
        }

        return result;
    } catch (const nlohmann::json::exception &e) {
        return make_error(ErrorCode::ParseError,
                          std::string("JSON 解析失败: ") + e.what());
    }
}

Result<std::vector<Model::Term>> parse_terms(const std::string &json) {
    try {
        auto j = nlohmann::json::parse(json);
        if (!j.is_array()) {
            return make_error(ErrorCode::ParseError, "学期数据必须是 JSON 数组");
        }
        std::vector<Model::Term> terms;
        terms.reserve(j.size());
        for (const auto &item : j) {
            Model::Term t;
            t.code     = item.value("code", "");
            t.name     = item.value("name", "");
            t.selected = item.value("selected", false);
            t.index    = item.value("index", 0);
            terms.push_back(std::move(t));
        }
        return terms;
    } catch (const nlohmann::json::exception &e) {
        return make_error(ErrorCode::ParseError,
                          std::string("JSON 解析失败: ") + e.what());
    }
}

Result<std::vector<Model::Week>> parse_weeks(const std::string &json) {
    try {
        auto j = nlohmann::json::parse(json);
        if (!j.is_array()) {
            return make_error(ErrorCode::ParseError, "周次数据必须是 JSON 数组");
        }
        std::vector<Model::Week> weeks;
        weeks.reserve(j.size());
        for (const auto &item : j) {
            Model::Week w;
            w.serial_number = item.value("serialNumber", 0);
            w.name          = item.value("name", "");
            w.start_date    = item.value("startDate", "");
            w.end_date      = item.value("endDate", "");
            w.is_current    = item.value("isCurrent", false);
            weeks.push_back(std::move(w));
        }
        return weeks;
    } catch (const nlohmann::json::exception &e) {
        return make_error(ErrorCode::ParseError,
                          std::string("JSON 解析失败: ") + e.what());
    }
}

} // namespace UBAANext::Parser
