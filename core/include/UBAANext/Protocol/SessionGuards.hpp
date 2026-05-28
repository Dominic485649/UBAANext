/**
 * @file SessionGuards.hpp
 * @brief Shared session-expiry and SSO marker detection helpers.
 *
 * PartiallyMigrated parser boundary：helpers inspect headers, URLs, and HTML snippets; raw inputs must not be logged.
 */
#pragma once

#include <UBAANext/Net/HttpResponse.hpp>

#include <string>

namespace UBAANext::Protocol {

/** Sensitive output: reads a response header such as Location/Set-Cookie; caller must redact before diagnostics. */
[[nodiscard]] std::string header_value(const HttpResponse &response, const std::string &name);
/** PartiallyMigrated marker detector: scans text for SSO login hints without logging raw HTML. */
[[nodiscard]] bool has_sso_login_marker(const std::string &text);
/** PartiallyMigrated marker detector: scans text for expired-session hints without logging raw body. */
[[nodiscard]] bool has_session_expired_marker(const std::string &text);
/** Sensitive URL detector: URL may contain ticket/token query parameters. */
[[nodiscard]] bool is_sso_or_login_url(const std::string &url);
/** PartiallyMigrated content detector: raw body must not be logged. */
[[nodiscard]] bool is_html_response(const std::string &body);
/** PartiallyMigrated session detector: maps common expired responses without swallowing downstream errors. */
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response);
/** PartiallyMigrated session detector: checks final URL/body while preserving redaction expectations. */
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response, const std::string &final_url, bool expect_json = false);

} // namespace UBAANext::Protocol
