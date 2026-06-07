#pragma once

#include <string>
#include <vector>

namespace UBAANext {
namespace Model {

struct LiveSchedule {
    std::string course_id;
    std::string live_id;
    std::string name;
    std::string teacher;
    std::string raw_status;
};

struct LiveWeekSchedule {
    std::string start_date;
    std::string end_date;
    std::vector<std::vector<LiveSchedule>> days;
};

} // namespace Model
} // namespace UBAANext
