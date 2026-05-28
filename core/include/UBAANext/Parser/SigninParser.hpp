#pragma once

#include <UBAANext/Model/Signin.hpp>

#include <nlohmann/json.hpp>

#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: parses today's sign-in course state; live iClass field drift remains possible. */
std::vector<Model::SigninCourse> parse_signin_today_courses(const nlohmann::json &response);

} // namespace Parser
} // namespace UBAANext
