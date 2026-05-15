#pragma once

#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>

#include <nlohmann/json.hpp>

#include <string>

namespace UBAANext {
namespace ServiceResponse {

[[nodiscard]] bool is_session_expired_response(const HttpResponse &response);
[[nodiscard]] bool is_session_expired_message(const std::string &message);
[[nodiscard]] bool envelope_ok(const nlohmann::json &json);
[[nodiscard]] std::string envelope_message(const nlohmann::json &json, const std::string &fallback);
[[nodiscard]] nlohmann::json envelope_data(const nlohmann::json &json);
[[nodiscard]] Result<nlohmann::json> parse_json_response(const HttpResponse &response, const std::string &context);

} // namespace ServiceResponse
} // namespace UBAANext
