#pragma once

#include <map>
#include <string>
#include <vector>

namespace UBAANext {
namespace Model {

struct EvaluationTask {
    std::string id;
    std::string title;
    std::string status;
    std::string teacher;
    std::string task_id;
    std::string questionnaire_id;
    std::string course_code;
    std::string teacher_code;
    std::string term_code;
    std::string pattern_id;
    std::string evaluator_code;
    std::string evaluator_name;
    std::string assignment_no;
    std::string year;
    std::string semester;
    std::string evaluation_type_id = "2";
    std::string allow_all = "1";
    std::string department_submit_status;
    int evaluated_count = 0;
    int required_count = 1;
};

struct EvaluationChoice {
    std::string id;
    std::string title;
    double score = 0.0;
};

struct EvaluationQuestion {
    std::string id;
    std::string title;
    std::string type;
    bool is_choice = false;
    std::vector<EvaluationChoice> choices;
};

struct EvaluationForm {
    EvaluationTask task;
    std::string form_result_id;
    std::string evaluator_role_id;
    std::string evaluated_relation_id;
    std::string result_map_id;
    std::string result_detail_map_id;
    std::map<std::string, std::string> submit_fields;
    std::map<std::string, std::string> result_map;
    std::vector<EvaluationQuestion> questions;
};

struct EvaluationAnswer {
    std::string question_id;
    std::string choice_id;
    std::string text;
};

struct EvaluationSubmission {
    std::string target_id;
    std::vector<EvaluationAnswer> answers;
    std::string reason;
};

} // namespace Model
} // namespace UBAANext
