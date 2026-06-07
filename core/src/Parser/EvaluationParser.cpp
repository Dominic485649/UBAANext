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

double json_double(const nlohmann::json &json, const char *key, double fallback = 0.0) {
    if (!json.contains(key) || json[key].is_null()) return fallback;
    if (json[key].is_number()) return json[key].get<double>();
    if (json[key].is_string()) {
        try { return std::stod(json[key].get<std::string>()); } catch (...) { return fallback; }
    }
    return fallback;
}

const nlohmann::json &form_object(const nlohmann::json &topic) {
    if (topic.is_array() && !topic.empty() && topic[0].is_object()) return topic[0];
    return topic;
}

void copy_submit_field(const nlohmann::json &source, std::map<std::string, std::string> &fields, const char *key) {
    auto value = json_string(source, key);
    if (!value.empty()) fields[key] = std::move(value);
}

void copy_all_string_fields(const nlohmann::json &source, std::map<std::string, std::string> &fields) {
    if (!source.is_object()) return;
    for (auto it = source.begin(); it != source.end(); ++it) {
        if (it->is_string()) fields[it.key()] = it->get<std::string>();
        else if (it->is_number_integer()) fields[it.key()] = std::to_string(it->get<long long>());
        else if (it->is_number_float()) fields[it.key()] = std::to_string(it->get<double>());
        else if (it->is_boolean()) fields[it.key()] = it->get<bool>() ? "1" : "0";
    }
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

Model::EvaluationForm parse_evaluation_form(const nlohmann::json &topic,
                                            const Model::EvaluationTask &task) {
    const auto &form_json = form_object(topic);
    Model::EvaluationForm form;
    form.task = task;
    form.submit_fields["rwid"] = task.task_id;
    form.submit_fields["wjid"] = task.questionnaire_id;
    form.submit_fields["kcdm"] = task.course_code;
    form.submit_fields["bpdm"] = task.teacher_code;
    form.submit_fields["xnxq"] = task.term_code;
    form.submit_fields["msid"] = task.pattern_id;
    form.submit_fields["ypjcs"] = std::to_string(task.evaluated_count);
    form.submit_fields["xypjcs"] = std::to_string(task.required_count);

    if (!form_json.is_object()) return form;

    const auto info = form_json.contains("pjxtPjjgPjjgckb") && form_json["pjxtPjjgPjjgckb"].is_array() && !form_json["pjxtPjjgPjjgckb"].empty() && form_json["pjxtPjjgPjjgckb"][0].is_object()
        ? form_json["pjxtPjjgPjjgckb"][0]
        : nlohmann::json::object();
    if (info.is_object()) {
        for (const auto *key : {"bprdm", "bprmc", "kcdm", "kcmc", "pjfs", "pjid", "pjlx", "pjrdm", "pjrjsdm", "pjrxm", "rwh", "wjssrwid", "xnxq", "sfxxpj"}) {
            copy_submit_field(info, form.submit_fields, key);
        }
        form.form_result_id = json_string(info, "pjid");
        form.evaluator_role_id = json_string(info, "pjrjsdm");
        form.evaluated_relation_id = json_string(info, "wjssrwid");
    }

    const auto map = form_json.contains("pjmap") && form_json["pjmap"].is_object() ? form_json["pjmap"] : nlohmann::json::object();
    if (map.is_object()) {
        copy_all_string_fields(map, form.result_map);
        for (const auto *key : {"PJJGXXBM", "RWID", "PJJGBM"}) copy_submit_field(map, form.submit_fields, key);
        form.result_detail_map_id = json_string(map, "PJJGXXBM");
        form.result_map_id = json_string(map, "PJJGBM");
    }

    const auto questionnaire = form_json.contains("pjxtWjWjbReturnEntity") && form_json["pjxtWjWjbReturnEntity"].is_object()
        ? form_json["pjxtWjWjbReturnEntity"]
        : nlohmann::json::object();
    const auto sections = questionnaire.contains("wjzblist") && questionnaire["wjzblist"].is_array()
        ? questionnaire["wjzblist"]
        : nlohmann::json::array();
    for (const auto &section : sections) {
        if (!section.is_object() || !section.contains("tklist") || !section["tklist"].is_array()) continue;
        for (const auto &question_json : section["tklist"]) {
            if (!question_json.is_object()) continue;
            Model::EvaluationQuestion question;
            question.id = json_string(question_json, "tmid");
            question.title = json_string(question_json, "tgmc");
            question.type = json_string(question_json, "tmlx");
            if (question.type.empty()) question.type = "1";
            const auto options = question_json.contains("tmxxlist") && question_json["tmxxlist"].is_array()
                ? question_json["tmxxlist"]
                : nlohmann::json::array();
            for (const auto &choice_json : options) {
                if (!choice_json.is_object()) continue;
                Model::EvaluationChoice choice;
                choice.id = json_string(choice_json, "tmxxid");
                choice.title = json_string(choice_json, "xxmc");
                if (choice.title.empty()) choice.title = json_string(choice_json, "tmxxmc");
                choice.score = json_double(choice_json, "xxfz");
                question.choices.push_back(std::move(choice));
            }
            question.is_choice = question.type != "6" && !question.choices.empty();
            if (!question.id.empty()) form.questions.push_back(std::move(question));
        }
    }

    return form;
}

} // namespace Parser
} // namespace UBAANext
