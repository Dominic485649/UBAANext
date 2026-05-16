#pragma once

#include <string>
#include <vector>

namespace UBAANext {
namespace Model {

struct JudgeCourse {
    std::string id;
    std::string name;
};

struct JudgeAssignmentSummary {
    std::string id;
    std::string course_id;
    std::string course_name;
    std::string title;
    std::string status;
    std::string start_time;
    std::string due_time;
    std::string max_score;
    std::string my_score;
    int total_problems = 0;
    int submitted_count = 0;
    std::string status_text;
};

struct JudgeAssignmentDetail {
    std::string id;
    std::string course_id;
    std::string course_name;
    std::string title;
    std::string status;
    std::string content;
    std::string start_time;
    std::string due_time;
    std::string max_score;
    std::string my_score;
    int total_problems = 0;
    int submitted_count = 0;
    std::string status_text;
};

} // namespace Model
} // namespace UBAANext
