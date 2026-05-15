#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct SpocAssignmentSummary {
    std::string id;
    std::string course_id;
    std::string course_name;
    std::string teacher;
    std::string title;
    std::string status;
    std::string start_time;
    std::string due_time;
    std::string score;
    std::string term_code;
    std::string term_name;
    std::string submission_status;
};

struct SpocAssignmentDetail {
    std::string id;
    std::string course_id;
    std::string title;
    std::string status;
    std::string start_time;
    std::string due_time;
    std::string score;
    std::string content;
    std::string submission_status;
    std::string submitted_at;
};

} // namespace Model
} // namespace UBAANext
