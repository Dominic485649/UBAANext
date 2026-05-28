#pragma once

#include <UBAANext/Model/Bykc.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** Unverified parser entry: maps BYKC profile JSON into the local model; live field drift remains possible. */
Model::BykcProfile parse_bykc_profile(const nlohmann::json &data);
/** Unverified parser entry: maps BYKC course list JSON; missing optional fields must not imply Aligned semantics. */
std::vector<Model::BykcCourse> parse_bykc_courses(const nlohmann::json &content);
/** Sensitive output: parses course detail/enrollment metadata for a specific course id. */
Model::BykcCourseDetail parse_bykc_course_detail(const nlohmann::json &data, const std::string &course_id);
/** Sensitive output: parses chosen-course enrollment state and must not be logged verbatim. */
std::vector<Model::BykcChosenCourse> parse_bykc_chosen_courses(const nlohmann::json &list);
/** Unverified parser entry: maps BYKC stats JSON; live field drift remains possible. */
std::vector<Model::BykcStat> parse_bykc_stats(const nlohmann::json &data);
/** PartiallyMigrated helper: normalizes BYKC status fields without proving all backend enum values. */
std::string bykc_course_status(const nlohmann::json &course);

} // namespace Parser
} // namespace UBAANext
