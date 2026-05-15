#pragma once

#include <UBAANext/Model/Bykc.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

Model::BykcProfile parse_bykc_profile(const nlohmann::json &data);
std::vector<Model::BykcCourse> parse_bykc_courses(const nlohmann::json &content);
Model::BykcCourseDetail parse_bykc_course_detail(const nlohmann::json &data, const std::string &course_id);
std::vector<Model::BykcChosenCourse> parse_bykc_chosen_courses(const nlohmann::json &list);
std::vector<Model::BykcStat> parse_bykc_stats(const nlohmann::json &data);
std::string bykc_course_status(const nlohmann::json &course);

} // namespace Parser
} // namespace UBAANext
