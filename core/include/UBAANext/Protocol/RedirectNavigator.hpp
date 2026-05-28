/**
 * @file RedirectNavigator.hpp
 * @brief Manual redirect navigation for CAS/downstream activation.
 *
 * PartiallyMigrated live boundary：follow_redirects may issue multiple remote requests and all URL/body/header diagnostics must be redacted.
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <functional>
#include <string>

namespace UBAANext::Protocol {

struct RedirectStep {
    /** Sensitive output: raw request URL may contain ticket/token query parameters. */
    std::string request_url;
    /** Sensitive output: resolved URL may contain WebVPN-encoded host/path/query data. */
    std::string resolved_url;
    /** Sensitive output: response headers/body may contain cookies, tokens, or raw HTML. */
    HttpResponse response;
};

struct RedirectNavigationResult {
    /** Sensitive output: final URL may contain ticket/token query parameters. */
    std::string final_url;
    /** Sensitive output: final response may include cookies or raw backend HTML. */
    HttpResponse final_response;
};

using RedirectStepObserver = std::function<Result<void>(const RedirectStep &step)>;
using UrlResolver = std::function<std::string(const std::string &url, ConnectionMode mode)>;
using RequestConfigurer = std::function<void(HttpRequest &request)>;

/** Sensitive URL helper: resolves Location against a base URL; callers must redact query strings before diagnostics. */
[[nodiscard]] std::string resolve_location(const std::string &base_url, const std::string &location);
/** Sensitive URL helper: extracts query parameters that may contain tickets/tokens. */
[[nodiscard]] std::string extract_query_parameter(const std::string &url, const std::string &name);
/** Sensitive URL helper: extracts query parameters from nested URLs and must not be logged verbatim. */
[[nodiscard]] std::string extract_query_parameter_anywhere(const std::string &url, const std::string &name);
/** Redaction helper: removes sensitive URL query values for diagnostics. */
[[nodiscard]] std::string redact_url_query(const std::string &url);

/** Transport boundary: disables adapter redirects so session navigation remains observable and redacted. */
void disable_transport_redirects(HttpRequest &request);

/** Remote request: yes. Follows downstream redirects and returns explicit session/network/protocol errors. */
[[nodiscard]] Result<RedirectNavigationResult> follow_redirects(IHttpClient &http_client,
                                                                ConnectionMode mode,
                                                                std::string initial_url,
                                                                DownstreamSystemId system,
                                                                UrlResolver resolve_url,
                                                                RequestConfigurer configure_request = {},
                                                                RedirectStepObserver observe_step = {},
                                                                int max_redirects = 12);

} // namespace UBAANext::Protocol
