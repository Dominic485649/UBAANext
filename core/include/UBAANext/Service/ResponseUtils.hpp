#pragma once

#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>

#include <nlohmann/json.hpp>

#include <string>

namespace UBAANext {
namespace ServiceResponse {

/** PartiallyMigrated helper: detects session-expired HTTP responses without exposing response bodies. */
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response);
/** PartiallyMigrated helper: detects backend session messages and must not log raw sensitive text. */
[[nodiscard]] bool is_session_expired_message(const std::string &message);
/** PartiallyMigrated envelope helper: checks common backend success flags; service-specific drift remains possible. */
[[nodiscard]] bool envelope_ok(const nlohmann::json &json);
/** Sensitive output: extracts a backend message for redaction-aware error paths only. */
[[nodiscard]] std::string envelope_message(const nlohmann::json &json, const std::string &fallback);
/** PartiallyMigrated envelope helper: returns common data payload variants without claiming domain completeness. */
[[nodiscard]] nlohmann::json envelope_data(const nlohmann::json &json);
/** Sensitive output: parses HTTP JSON and maps session/network/parse failures to stable Result errors. */
[[nodiscard]] Result<nlohmann::json> parse_json_response(const HttpResponse &response, const std::string &context);

} // namespace ServiceResponse
} // namespace UBAANext
