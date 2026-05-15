#pragma once

#include <string>

namespace UBAANext {
namespace Model {

struct VenueSite {
    std::string id;
    std::string name;
    std::string venue_id;
    std::string venue_name;
    std::string campus_name;
};

struct VenuePurposeType {
    std::string id;
    std::string name;
};

struct VenueSpaceInfo {
    std::string id;
    std::string name;
    std::string date;
    std::string site_id;
    std::string token;
};

struct VenueOrder {
    std::string id;
    std::string title;
    std::string status;
    std::string reservation_date;
    std::string space;
    std::string site;
    std::string phone;
    std::string joiners;
};

} // namespace Model
} // namespace UBAANext
