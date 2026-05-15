#pragma once

#include <string>

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
    int evaluated_count = 0;
    int required_count = 1;
};

} // namespace Model
} // namespace UBAANext
