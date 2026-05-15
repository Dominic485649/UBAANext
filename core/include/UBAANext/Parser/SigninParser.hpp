#pragma once

#include <UBAANext/Model/Signin.hpp>

#include <nlohmann/json.hpp>

#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::SigninCourse> parse_signin_today_courses(const nlohmann::json &response);

} // namespace Parser
} // namespace UBAANext
