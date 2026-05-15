#pragma once

#include <UBAANext/Model/Spoc.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::SpocAssignmentSummary> parse_spoc_assignments_page(const nlohmann::json &page,
                                                                      const std::map<std::string, std::pair<std::string, std::string>> &courses,
                                                                      const std::string &term_code,
                                                                      const std::string &term_name);
Model::SpocAssignmentDetail parse_spoc_assignment_detail(const nlohmann::json &detail,
                                                         const nlohmann::json *submission,
                                                         const std::string &assignment_id);

} // namespace Parser
} // namespace UBAANext
