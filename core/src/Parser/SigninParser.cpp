#include <UBAANext/Parser/SigninParser.hpp>

#include <string>
#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    return {};
}

std::string normalize_signin_status(const std::string &sign_status) {
    return sign_status == "1" || sign_status == "signed" || sign_status == "success" ? "signed" : "available";
}

Model::SigninCourse parse_schedule_item(const nlohmann::json &item) {
    Model::SigninCourse record;
    record.id = json_string(item, "id");
    if (record.id.empty()) record.id = json_string(item, "courseSchedId");
    record.course_id = json_string(item, "courseId");
    if (record.course_id.empty()) record.course_id = json_string(item, "course_id");
    record.name = json_string(item, "courseName");
    if (record.name.empty()) record.name = json_string(item, "course_name");
    record.teacher = json_string(item, "teacherName");
    if (record.teacher.empty()) record.teacher = json_string(item, "teacher_name");
    record.sign_status = json_string(item, "signStatus");
    record.status = normalize_signin_status(record.sign_status);
    record.class_begin_time = json_string(item, "classBeginTime");
    record.class_end_time = json_string(item, "classEndTime");
    if (record.id.empty()) record.id = record.course_id.empty() ? record.name : record.course_id;
    if (record.name.empty()) record.name = "签到课程";
    return record;
}

} // namespace

std::vector<Model::SigninCourse> parse_signin_today_courses(const nlohmann::json &response) {
    return parse_signin_schedule(response);
}

std::vector<Model::SigninCourse> parse_signin_schedule(const nlohmann::json &response) {
    std::vector<Model::SigninCourse> records;
    if (!response.contains("result") || !response["result"].is_array()) return records;
    for (const auto &item : response["result"]) {
        if (!item.is_object()) continue;
        auto record = parse_schedule_item(item);
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::SigninTermCourse> parse_signin_term_courses(const nlohmann::json &response) {
    std::vector<Model::SigninTermCourse> records;
    if (!response.contains("result") || !response["result"].is_array()) return records;
    for (const auto &item : response["result"]) {
        if (!item.is_object()) continue;
        Model::SigninTermCourse record;
        record.id = json_string(item, "course_id");
        if (record.id.empty()) record.id = json_string(item, "courseId");
        record.name = json_string(item, "course_name");
        if (record.name.empty()) record.name = json_string(item, "courseName");
        record.teacher = json_string(item, "teacher_name");
        if (record.teacher.empty()) record.teacher = json_string(item, "teacherName");
        if (!record.id.empty() && !record.teacher.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::SigninCourse> parse_signin_course_schedule(const nlohmann::json &response) {
    return parse_signin_schedule(response);
}

} // namespace Parser
} // namespace UBAANext
