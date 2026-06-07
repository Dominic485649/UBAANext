#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct SigninCourse {
    std::string id;
    std::string course_id;
    std::string name;
    std::string teacher;
    std::string status;
    std::string class_begin_time;
    std::string class_end_time;
    std::string sign_status;
};

struct SigninTermCourse {
    std::string id;
    std::string name;
    std::string teacher;
};

} // namespace Model
} // namespace UBAANext
