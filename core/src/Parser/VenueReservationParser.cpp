#include <UBAANext/Parser/VenueReservationParser.hpp>

#include <functional>
#include <map>
#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    return {};
}

std::string order_title(const nlohmann::json &order) {
    auto title = json_string(order, "theme");
    return title.empty() ? json_string(order, "venueSpaceName") : title;
}

bool json_bool(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return false;
    if (json[key].is_boolean()) return json[key].get<bool>();
    if (json[key].is_number_integer()) return json[key].get<int>() != 0;
    if (json[key].is_string()) return json[key].get<std::string>() == "true" || json[key].get<std::string>() == "1";
    return false;
}

bool slot_is_reservable(const nlohmann::json &slot) {
    auto status = json_string(slot, "reservationStatus");
    return status == "1" && json_string(slot, "tradeNo").empty() && json_string(slot, "orderId").empty() && !json_bool(slot, "takeUp");
}

std::string slot_label(const nlohmann::json &slot) {
    auto begin = json_string(slot, "beginTime");
    auto end = json_string(slot, "endTime");
    return begin.empty() || end.empty() ? json_string(slot, "id") : begin + "-" + end;
}

} // namespace

std::vector<Model::VenueSite> parse_venue_sites(const nlohmann::json &data) {
    auto content = data.contains("content") && data["content"].is_array() ? data["content"] : data.is_array() ? data : nlohmann::json::array();
    std::vector<Model::VenueSite> records;
    for (const auto &venue : content) {
        auto venue_id = json_string(venue, "id");
        auto venue_name = json_string(venue, "venueName");
        auto campus = json_string(venue, "campusName");
        if (!venue.contains("siteList") || !venue["siteList"].is_array()) continue;
        for (const auto &site : venue["siteList"]) {
            Model::VenueSite record;
            record.id = json_string(site, "id");
            record.name = json_string(site, "siteName");
            record.venue_id = venue_id;
            record.venue_name = venue_name;
            record.campus_name = campus;
            if (!record.id.empty()) records.push_back(std::move(record));
        }
    }
    return records;
}

std::vector<Model::VenuePurposeType> parse_venue_purpose_types(const nlohmann::json &data) {
    std::vector<Model::VenuePurposeType> records;
    std::function<void(const nlohmann::json &)> visit = [&](const nlohmann::json &node) {
        if (node.is_array()) {
            for (const auto &child : node) visit(child);
            return;
        }
        if (!node.is_object()) return;
        auto name = json_string(node, "name");
        auto id = json_string(node, "key");
        if (id.empty()) id = json_string(node, "value");
        if (id.empty()) id = json_string(node, "id");
        if (!id.empty() && name.find("类") != std::string::npos) records.push_back({id, name});
        for (const auto &[_, child] : node.items()) visit(child);
    };
    visit(data);
    return records;
}

std::vector<Model::VenueSpaceInfo> parse_venue_day_info(const nlohmann::json &data, const std::string &site_id) {
    std::vector<Model::VenueSpaceInfo> records;
    auto spaces_by_date = data.contains("reservationDateSpaceInfo") && data["reservationDateSpaceInfo"].is_object() ? data["reservationDateSpaceInfo"] : nlohmann::json::object();
    auto token = json_string(data, "token");
    std::map<std::string, nlohmann::json> slots;
    if (data.contains("spaceTimeInfo") && data["spaceTimeInfo"].is_array()) {
        for (const auto &slot : data["spaceTimeInfo"]) {
            auto id = json_string(slot, "id");
            if (!id.empty()) slots[id] = slot;
        }
    }
    for (const auto &[day, spaces] : spaces_by_date.items()) {
        if (!spaces.is_array()) continue;
        for (const auto &space : spaces) {
            auto space_id = json_string(space, "id");
            auto space_name = json_string(space, "spaceName");
            if (space_id.empty()) continue;
            bool emitted_slot = false;
            for (const auto &[time_id, slot] : slots) {
                if (!space.contains(time_id) || !space[time_id].is_object()) continue;
                const auto &space_slot = space[time_id];
                Model::VenueSpaceInfo record;
                record.id = space_id + ":" + time_id;
                record.name = space_name;
                record.date = day;
                record.site_id = site_id;
                record.token = token;
                record.time_id = time_id;
                record.time_label = slot_label(slot);
                record.status = json_string(space_slot, "reservationStatus");
                record.reservable = slot_is_reservable(space_slot) ? "true" : "false";
                records.push_back(std::move(record));
                emitted_slot = true;
            }
            if (!emitted_slot) {
                Model::VenueSpaceInfo record;
                record.id = space_id;
                record.name = space_name;
                record.date = day;
                record.site_id = site_id;
                record.token = token;
                if (!record.id.empty()) records.push_back(std::move(record));
            }
        }
    }
    return records;
}

std::vector<Model::VenueOrder> parse_venue_orders(const nlohmann::json &data) {
    auto content = data.contains("content") && data["content"].is_array() ? data["content"] : nlohmann::json::array();
    std::vector<Model::VenueOrder> records;
    for (const auto &order : content) {
        records.push_back(parse_venue_order_detail(order, json_string(order, "id")));
    }
    return records;
}

Model::VenueOrder parse_venue_order_detail(const nlohmann::json &data, const std::string &order_id) {
    Model::VenueOrder record;
    record.id = order_id.empty() ? json_string(data, "id") : order_id;
    record.title = order_title(data);
    record.status = json_string(data, "orderStatus");
    record.reservation_date = json_string(data, "reservationDate");
    record.space = json_string(data, "venueSpaceName");
    record.site = json_string(data, "siteName");
    record.phone = json_string(data, "phone");
    record.joiners = json_string(data, "joiners");
    return record;
}

} // namespace Parser
} // namespace UBAANext
