#include <UBAANext/Parser/BykcParser.hpp>

#include <charconv>
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

std::string json_string_or_dump(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_object() || json[key].is_array()) return json[key].dump();
    return json_string(json, key);
}

std::string nested_string(const nlohmann::json &json, const char *object_key, const char *key) {
    if (!json.contains(object_key) || !json[object_key].is_object()) return {};
    return json_string(json[object_key], key);
}

bool parse_int(const std::string &value, int &out) {
    auto result = std::from_chars(value.data(), value.data() + value.size(), out);
    return !value.empty() && result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

} // namespace

std::string bykc_course_status(const nlohmann::json &course) {
    if (course.value("selected", false)) return "selected";
    auto max_count = json_string(course, "courseMaxCount");
    auto current = json_string(course, "courseCurrentCount");
    if (!max_count.empty() && !current.empty()) {
        int parsed_current = 0;
        int parsed_max_count = 0;
        if (parse_int(current, parsed_current) && parse_int(max_count, parsed_max_count) && parsed_current >= parsed_max_count) return "full";
    }
    return "available";
}

Model::BykcProfile parse_bykc_profile(const nlohmann::json &data) {
    Model::BykcProfile profile;
    profile.id = json_string(data, "id");
    profile.real_name = json_string(data, "realName");
    profile.employee_id = json_string(data, "employeeId");
    profile.student_no = json_string(data, "studentNo");
    profile.student_type = json_string(data, "studentType");
    profile.class_code = json_string(data, "classCode");
    profile.college_name = nested_string(data, "college", "collegeName");
    profile.term_name = nested_string(data, "term", "termName");
    return profile;
}

std::vector<Model::BykcCourse> parse_bykc_courses(const nlohmann::json &content) {
    const auto &list = content.is_array() ? content : nlohmann::json::array();
    std::vector<Model::BykcCourse> courses;
    for (const auto &course : list) {
        Model::BykcCourse record;
        record.id = json_string(course, "id");
        record.name = json_string(course, "courseName");
        record.status = bykc_course_status(course);
        record.teacher = json_string(course, "courseTeacher");
        record.position = json_string(course, "coursePosition");
        record.start_date = json_string(course, "courseStartDate");
        record.end_date = json_string(course, "courseEndDate");
        record.select_start_date = json_string(course, "courseSelectStartDate");
        record.select_end_date = json_string(course, "courseSelectEndDate");
        record.cancel_end_date = json_string(course, "courseCancelEndDate");
        record.max_count = json_string(course, "courseMaxCount");
        record.current_count = json_string(course, "courseCurrentCount");
        record.category = nested_string(course, "courseNewKind1", "kindName");
        record.sub_category = nested_string(course, "courseNewKind2", "kindName");
        record.selected = json_string(course, "selected");
        if (!record.id.empty()) courses.push_back(std::move(record));
    }
    return courses;
}

Model::BykcCourseDetail parse_bykc_course_detail(const nlohmann::json &data, const std::string &course_id) {
    Model::BykcCourseDetail detail;
    detail.id = course_id.empty() ? json_string(data, "id") : course_id;
    detail.name = json_string(data, "courseName");
    detail.status = bykc_course_status(data);
    detail.teacher = json_string(data, "courseTeacher");
    detail.position = json_string(data, "coursePosition");
    detail.contact = json_string(data, "courseContact");
    detail.mobile = json_string(data, "courseContactMobile");
    detail.description = json_string(data, "courseDesc");
    detail.start_date = json_string(data, "courseStartDate");
    detail.end_date = json_string(data, "courseEndDate");
    detail.selected = json_string(data, "selected");
    detail.sign_config = json_string_or_dump(data, "courseSignConfig");
    return detail;
}

std::vector<Model::BykcChosenCourse> parse_bykc_chosen_courses(const nlohmann::json &list) {
    std::vector<Model::BykcChosenCourse> records;
    if (!list.is_array()) return records;
    for (const auto &chosen_course : list) {
        auto course = chosen_course.contains("courseInfo") && chosen_course["courseInfo"].is_object() ? chosen_course["courseInfo"] : nlohmann::json::object();
        Model::BykcChosenCourse record;
        record.id = json_string(chosen_course, "id");
        record.course_id = json_string(course, "id");
        record.name = json_string(course, "courseName");
        record.teacher = json_string(course, "courseTeacher");
        record.position = json_string(course, "coursePosition");
        record.select_date = json_string(chosen_course, "selectDate");
        record.checkin = json_string(chosen_course, "checkin");
        record.pass = json_string(chosen_course, "pass");
        record.score = json_string(chosen_course, "score");
        record.homework = json_string(chosen_course, "homework");
        record.sign_info = json_string(chosen_course, "signInfo");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::BykcStat> parse_bykc_stats(const nlohmann::json &data) {
    std::vector<Model::BykcStat> records;
    Model::BykcStat total;
    total.id = "total";
    total.title = "累计有效修读";
    total.valid_count = json_string(data, "validCount");
    records.push_back(std::move(total));
    if (data.contains("statistical") && data["statistical"].is_object()) {
        for (const auto &[category, sub_map] : data["statistical"].items()) {
            if (!sub_map.is_object()) continue;
            for (const auto &[sub_category, entry] : sub_map.items()) {
                Model::BykcStat stat;
                stat.id = category + ":" + sub_category;
                stat.title = sub_category;
                stat.category = category;
                stat.required_count = json_string(entry, "assessmentCount");
                stat.passed_count = json_string(entry, "completeAssessmentCount");
                records.push_back(std::move(stat));
            }
        }
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
