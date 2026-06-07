#pragma once

#include <string>

namespace UBAANext::Model {

struct SrsCourseFilter {
    std::string scope = "TJKC";
    int page = 1;
    int size = 20;
    int campus = 1;
    bool display_conflict = false;
    std::string requirement;
    std::string category;
    std::string keyword;
};

struct SrsCourseOperation {
    std::string scope;
    std::string class_id;
    std::string secret;
    std::string batch_id;
    int volunteer_index = 0;
};

} // namespace UBAANext::Model
