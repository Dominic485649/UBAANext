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

} // namespace

std::vector<Model::SigninCourse> parse_signin_today_courses(const nlohmann::json &response) {
    std::vector<Model::SigninCourse> records;
    if (!response.contains("result") || !response["result"].is_array()) return records;
    for (const auto &item : response["result"]) {
        if (!item.is_object()) continue;
        Model::SigninCourse record;
        record.id = json_string(item, "id");
        record.name = json_string(item, "courseName");
        if (record.id.empty()) record.id = record.name;
        if (record.name.empty()) record.name = "签到课程";
        record.sign_status = json_string(item, "signStatus");
        record.status = record.sign_status == "1" ? "signed" : "available";
        record.class_begin_time = json_string(item, "classBeginTime");
        record.class_end_time = json_string(item, "classEndTime");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
