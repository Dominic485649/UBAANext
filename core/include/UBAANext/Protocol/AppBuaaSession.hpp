/**
 * @file AppBuaaSession.hpp
 * @brief AppBuaa downstream session activation helpers.
 *
 * PartiallyMigrated live boundary：these helpers may trigger remote redirects/auth requests and must not log credentials, cookies, or raw HTML.
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <string>

namespace UBAANext::Protocol::AppBuaa {

/** PartiallyMigrated URL resolver for Direct/WebVPN AppBuaa requests; diagnostics must redact query strings. */
std::string resolve_url(const std::string &url, ConnectionMode mode);
/** Sensitive transport helper: applies AJAX headers for authenticated AppBuaa requests. */
void apply_ajax_headers(HttpRequest &request,
                        ConnectionMode mode,
                        const std::string &referer = "https://app.buaa.edu.cn/",
                        const std::string &user_agent = "UBAANext/0.4");
/** PartiallyMigrated session detector: identifies expired AppBuaa responses without exposing raw body. */
bool is_session_expired_response(const HttpResponse &response);
/** Remote request: yes. Ensures AppBuaa session via redirects; returns stable session/network/parse errors. */
Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent = "UBAANext/0.4");
/** Sensitive input + remote request: yes. Ensures AppBuaa session using credentials; credentials must never be logged. */
Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent,
                            const std::string &username,
                            const std::string &password);

} // namespace UBAANext::Protocol::AppBuaa
