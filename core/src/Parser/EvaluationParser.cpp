#include <UBAANext/Parser/EvaluationParser.hpp>

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

int json_int(const nlohmann::json &json, const char *key, int fallback = 0) {
    if (!json.contains(key) || json[key].is_null()) return fallback;
    if (json[key].is_number_integer()) return json[key].get<int>();
    if (json[key].is_string()) {
        try { return std::stoi(json[key].get<std::string>()); } catch (...) { return fallback; }
    }
    return fallback;
}

} // namespace

std::vector<Model::EvaluationTask> parse_evaluation_required_reviews(const nlohmann::json &courses,
                                                                      const std::string &task_id,
                                                                      const std::string &questionnaire_id,
                                                                      const std::string &pattern_id,
                                                                      const std::string &term_code,
                                                                      const std::string &status) {
    std::vector<Model::EvaluationTask> records;
    if (!courses.is_array()) return records;
    for (const auto &course : courses) {
        auto course_code = json_string(course, "kcdm");
        auto teacher_code = json_string(course, "bpdm");
        if (course_code.empty()) continue;
        Model::EvaluationTask record;
        record.id = task_id + "_" + questionnaire_id + "_" + course_code + "_" + teacher_code;
        record.title = json_string(course, "kcmc").empty() ? "未知课程" : json_string(course, "kcmc");
        record.status = status;
        record.teacher = json_string(course, "bpmc");
        record.task_id = task_id;
        record.questionnaire_id = questionnaire_id;
        record.course_code = course_code;
        record.teacher_code = teacher_code;
        record.term_code = term_code;
        record.pattern_id = pattern_id;
        record.evaluator_code = json_string(course, "pjrdm");
        record.evaluator_name = json_string(course, "pjrmc");
        record.assignment_no = json_string(course, "rwh");
        record.year = json_string(course, "xn");
        record.semester = json_string(course, "xq");
        record.evaluation_type_id = json_string(course, "pjlxid").empty() ? "2" : json_string(course, "pjlxid");
        record.allow_all = json_string(course, "sfksqbpj").empty() ? "1" : json_string(course, "sfksqbpj");
        record.department_submit_status = json_string(course, "yxsfktjst");
        record.evaluated_count = json_int(course, "ypjcs");
        record.required_count = json_int(course, "xypjcs", 1);
        records.push_back(std::move(record));
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
