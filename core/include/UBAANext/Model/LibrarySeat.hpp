#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct LibraryInfo {
    std::string id;
    std::string name;
    std::string free_num;
    std::string total_num;
};

struct LibraryArea {
    std::string id;
    std::string name;
    std::string area;
    std::string premises_id;
    std::string storey_id;
    std::string free_num;
    std::string total_num;
    std::string available_dates;
};

struct LibrarySeat {
    std::string id;
    std::string title;
    std::string status;
    std::string name;
    std::string raw_status;
    std::string status_name;
};

struct LibraryReservation {
    std::string id;
    std::string title;
    std::string status;
    std::string area_name;
    std::string day;
    std::string begin_time;
    std::string end_time;
    std::string status_name;
};

} // namespace Model
} // namespace UBAANext
