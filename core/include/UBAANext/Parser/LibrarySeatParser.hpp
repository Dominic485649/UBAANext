#pragma once

#include <UBAANext/Model/LibrarySeat.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::LibraryInfo> parse_library_infos(const nlohmann::json &list);
std::vector<Model::LibraryArea> parse_library_areas(const nlohmann::json &list);
Model::LibraryArea parse_library_area_detail(const nlohmann::json &data, const std::string &area_id);
std::vector<Model::LibrarySeat> parse_library_seats(const nlohmann::json &list);
std::vector<Model::LibraryReservation> parse_library_reservations(const nlohmann::json &list);

} // namespace Parser
} // namespace UBAANext
