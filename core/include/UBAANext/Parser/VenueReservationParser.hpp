#pragma once

#include <UBAANext/Model/VenueReservation.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::VenueSite> parse_venue_sites(const nlohmann::json &data);
std::vector<Model::VenuePurposeType> parse_venue_purpose_types(const nlohmann::json &data);
std::vector<Model::VenueSpaceInfo> parse_venue_day_info(const nlohmann::json &data, const std::string &site_id);
std::vector<Model::VenueOrder> parse_venue_orders(const nlohmann::json &data);
Model::VenueOrder parse_venue_order_detail(const nlohmann::json &data, const std::string &order_id);

} // namespace Parser
} // namespace UBAANext
