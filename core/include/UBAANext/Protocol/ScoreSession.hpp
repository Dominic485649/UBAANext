/**
 * @file ScoreSession.hpp
 * @brief Score downstream session activation helpers.
 *
 * PartiallyMigrated live boundary：helpers may trigger remote requests and must keep cookies, ticket URLs, and raw bodies redacted.
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <string>

namespace UBAANext::Protocol::Score {

/** PartiallyMigrated URL resolver for Direct/WebVPN score requests; diagnostics must redact query strings. */
std::string resolve_url(const std::string &url, ConnectionMode mode);
/** Sensitive transport helper: applies authenticated score form headers. */
void apply_form_headers(HttpRequest &request);
/** PartiallyMigrated session detector: checks expired score responses without logging raw body. */
bool is_session_expired_response(const HttpResponse &response);
/** Remote request: yes. Ensures score session and returns stable session/network/parse errors. */
Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode);

} // namespace UBAANext::Protocol::Score
