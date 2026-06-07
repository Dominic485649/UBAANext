#pragma once

#include <UBAANext/Model/Signin.hpp>

#include <nlohmann/json.hpp>

#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: parses today's sign-in course state; live iClass field drift remains possible. */
std::vector<Model::SigninCourse> parse_signin_today_courses(const nlohmann::json &response);
/** ReadOnlyCandidate parser entry: parses a specific day's sign-in schedule. */
std::vector<Model::SigninCourse> parse_signin_schedule(const nlohmann::json &response);
/** ReadOnlyCandidate parser entry: parses all sign-in courses for a term. */
std::vector<Model::SigninTermCourse> parse_signin_term_courses(const nlohmann::json &response);
/** ReadOnlyCandidate parser entry: parses one course's sign-in schedule details. */
std::vector<Model::SigninCourse> parse_signin_course_schedule(const nlohmann::json &response);

} // namespace Parser
} // namespace UBAANext
