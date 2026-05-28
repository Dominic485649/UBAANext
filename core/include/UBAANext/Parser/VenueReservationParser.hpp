#pragma once

#include <UBAANext/Model/VenueReservation.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: maps venue sites; live field drift remains possible. */
std::vector<Model::VenueSite> parse_venue_sites(const nlohmann::json &data);
/** ReadOnlyCandidate parser entry: maps venue purpose types; backend enum drift remains possible. */
std::vector<Model::VenuePurposeType> parse_venue_purpose_types(const nlohmann::json &data);
/** Sensitive output: parses day-space availability for a site; capacity is volatile. */
std::vector<Model::VenueSpaceInfo> parse_venue_day_info(const nlohmann::json &data, const std::string &site_id);
/** Sensitive output: parses venue orders and booking metadata. */
std::vector<Model::VenueOrder> parse_venue_orders(const nlohmann::json &data);
/** Sensitive output: parses one venue order detail including lock/order state. */
Model::VenueOrder parse_venue_order_detail(const nlohmann::json &data, const std::string &order_id);

} // namespace Parser
} // namespace UBAANext
