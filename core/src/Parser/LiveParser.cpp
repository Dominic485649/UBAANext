#include <UBAANext/Parser/LiveParser.hpp>

#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    if (json[key].is_boolean()) return json[key].get<bool>() ? "true" : "false";
    return {};
}

nlohmann::json normalize_day_courses(const nlohmann::json &day) {
    if (day.is_array()) return day;
    if (day.is_object()) {
        if (day.contains("course") && day["course"].is_array()) return day["course"];
        if (day.contains("courses") && day["courses"].is_array()) return day["courses"];
        if (day.contains("list") && day["list"].is_array()) return day["list"];
    }
    return nlohmann::json::array();
}

} // namespace

std::vector<std::vector<Model::LiveSchedule>> parse_live_week_schedule_days(const nlohmann::json &list) {
    std::vector<std::vector<Model::LiveSchedule>> days;
    if (!list.is_array()) return days;
    days.reserve(list.size());
    for (const auto &day : list) {
        std::vector<Model::LiveSchedule> schedules;
        for (const auto &item : normalize_day_courses(day)) {
            Model::LiveSchedule schedule;
            schedule.course_id = json_string(item, "course_id");
            if (schedule.course_id.empty()) schedule.course_id = json_string(item, "courseId");
            schedule.live_id = json_string(item, "id");
            if (schedule.live_id.empty()) schedule.live_id = json_string(item, "live_id");
            schedule.name = json_string(item, "course_title");
            if (schedule.name.empty()) schedule.name = json_string(item, "courseTitle");
            if (schedule.name.empty()) schedule.name = json_string(item, "name");
            schedule.teacher = json_string(item, "teacher_name");
            if (schedule.teacher.empty()) schedule.teacher = json_string(item, "teacherName");
            if (schedule.teacher.empty()) schedule.teacher = json_string(item, "teacher");
            schedule.raw_status = json_string(item, "status");
            if (!schedule.course_id.empty() || !schedule.live_id.empty() || !schedule.name.empty()) schedules.push_back(std::move(schedule));
        }
        days.push_back(std::move(schedules));
    }
    return days;
}

} // namespace Parser
} // namespace UBAANext
