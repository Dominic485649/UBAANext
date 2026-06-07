#pragma once

#include <UBAANext/Model/Spoc.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: parses current SPOC teaching week configuration. */
Model::SpocWeek parse_spoc_week(const nlohmann::json &content);
/** ReadOnlyCandidate parser entry: parses SPOC weekly schedule items. */
std::vector<Model::SpocSchedule> parse_spoc_schedule(const nlohmann::json &content);
/** ReadOnlyCandidate parser entry: parses SPOC course list for a term. */
std::vector<Model::SpocCourse> parse_spoc_courses(const nlohmann::json &content);
/** PartiallyMigrated parser entry: parses one SPOC assignment page; pending/expired field semantics remain unverified. */
std::vector<Model::SpocAssignmentSummary> parse_spoc_assignments_page(const nlohmann::json &page,
                                                                      const std::map<std::string, std::pair<std::string, std::string>> &courses,
                                                                      const std::string &term_code,
                                                                      const std::string &term_name);
/** Sensitive output: parses SPOC assignment detail/submission metadata and must not log raw JSON. */
Model::SpocAssignmentDetail parse_spoc_assignment_detail(const nlohmann::json &detail,
                                                         const nlohmann::json *submission,
                                                         const std::string &assignment_id);

} // namespace Parser
} // namespace UBAANext
