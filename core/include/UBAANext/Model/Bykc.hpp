#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct BykcProfile {
    std::string id;
    std::string real_name;
    std::string employee_id;
    std::string student_no;
    std::string student_type;
    std::string class_code;
    std::string college_name;
    std::string term_name;
};

struct BykcCourse {
    std::string id;
    std::string name;
    std::string status;
    std::string teacher;
    std::string position;
    std::string start_date;
    std::string end_date;
    std::string select_start_date;
    std::string select_end_date;
    std::string cancel_end_date;
    std::string max_count;
    std::string current_count;
    std::string category;
    std::string sub_category;
    std::string selected;
};

struct BykcCourseDetail {
    std::string id;
    std::string name;
    std::string status;
    std::string teacher;
    std::string position;
    std::string contact;
    std::string mobile;
    std::string description;
    std::string start_date;
    std::string end_date;
    std::string selected;
    std::string sign_config;
};

struct BykcChosenCourse {
    std::string id;
    std::string course_id;
    std::string name;
    std::string teacher;
    std::string position;
    std::string select_date;
    std::string checkin;
    std::string pass;
    std::string score;
    std::string homework;
    std::string sign_info;
};

struct BykcStat {
    std::string id;
    std::string title;
    std::string category;
    std::string required_count;
    std::string passed_count;
    std::string valid_count;
};

} // namespace Model
} // namespace UBAANext
