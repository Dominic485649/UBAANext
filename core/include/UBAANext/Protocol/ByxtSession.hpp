/**
 * @file ByxtSession.hpp
 * @brief BYXT downstream session activation helpers.
 *
 * PartiallyMigrated live boundary：helpers may trigger remote requests and must keep cookies, ticket URLs, and raw bodies redacted.
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <string>

namespace UBAANext::Protocol::Byxt {

/** PartiallyMigrated URL resolver for Direct/WebVPN BYXT requests; diagnostics must redact query strings. */
std::string resolve_url(const std::string &url, ConnectionMode mode);
/** Sensitive transport helper: applies authenticated BYXT AJAX headers. */
void apply_ajax_headers(HttpRequest &request, ConnectionMode mode, const std::string &referer = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/index.html");
/** PartiallyMigrated session detector: checks expired BYXT responses without logging raw body. */
bool is_session_expired_response(const HttpResponse &response);
/** Remote request: yes. Ensures BYXT session and returns stable session/network/parse errors. */
Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode);

} // namespace UBAANext::Protocol::Byxt
