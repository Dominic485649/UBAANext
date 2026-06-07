#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct SpocWeek {
    std::string term_code;
    std::string start_date;
    std::string end_date;
    std::string raw_date_range;
};

struct SpocSchedule {
    std::string id;
    std::string course_id;
    std::string course_name;
    std::string teacher;
    std::string weekday;
    std::string classroom;
    std::string time_text;
    std::string start_time;
    std::string end_time;
};

struct SpocCourse {
    std::string id;
    std::string name;
    std::string teacher;
};

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

struct SpocHomeworkSubmission {
    std::string assignment_id;
    std::string course_id;
    std::string file_id;
    std::string file_name;
};

} // namespace Model
} // namespace UBAANext
