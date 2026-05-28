#pragma once

#include <UBAANext/Model/LibrarySeat.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: maps library list JSON; live field drift remains possible. */
std::vector<Model::LibraryInfo> parse_library_infos(const nlohmann::json &list);
/** ReadOnlyCandidate parser entry: maps library area JSON; capacity fields are volatile. */
std::vector<Model::LibraryArea> parse_library_areas(const nlohmann::json &list);
/** Sensitive output: parses one library area detail and seat/capacity metadata. */
Model::LibraryArea parse_library_area_detail(const nlohmann::json &data, const std::string &area_id);
/** Sensitive output: parses seat availability; availability must not be treated as cached truth. */
std::vector<Model::LibrarySeat> parse_library_seats(const nlohmann::json &list);
/** Sensitive output: parses user reservations and booking state. */
std::vector<Model::LibraryReservation> parse_library_reservations(const nlohmann::json &list);

} // namespace Parser
} // namespace UBAANext
