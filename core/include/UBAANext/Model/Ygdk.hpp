#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct YgdkClassify {
    std::string id;
    std::string name;
};

struct YgdkItem {
    std::string id;
    std::string name;
    std::string classify_id;
    std::string sort;
};

struct YgdkOverview {
    YgdkClassify classify;
    std::string term_name;
    std::string term_count;
    std::string term_good_count;
    std::string week_count;
    std::string month_count;
    std::string day_count;
};

struct YgdkRecord {
    std::string id;
    std::string item_name;
    std::string state;
    std::string place;
    std::string start_time;
    std::string end_time;
    std::string created_at;
};

} // namespace Model
} // namespace UBAANext
