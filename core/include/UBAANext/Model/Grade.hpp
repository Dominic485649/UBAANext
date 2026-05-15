#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct Grade {
    std::string id;
    std::string course_name;
    std::string course_code;
    std::string course_type;
    std::string credit;
    std::string score;
    std::string grade_point;
    std::string term_code;
    std::string raw_status;
};

} // namespace Model
} // namespace UBAANext
