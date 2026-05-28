/**
 * @file AuthorizedDownstreamRequestExecutor.hpp
 * @brief Authorized downstream request retry boundary.
 *
 * PartiallyMigrated live boundary：may refresh sessions, retry remote requests, and must propagate downstream failures without returning fake success.
 */
#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <functional>
#include <string>

namespace UBAANext::Protocol {

struct AuthorizedRequestHooks {
    /** Remote request may happen: true when authorization refresh is required. */
    std::function<Result<void>(bool force_refresh)> ensure_authorized;
    /** Sensitive session boundary: invalidates local downstream session state. */
    std::function<void()> invalidate;
    /** Sensitive input: decorates request with cookies/tokens/headers. */
    std::function<void(HttpRequest &request)> decorate_request;
    /** Sensitive output: inspects response without logging raw body/header values. */
    std::function<bool(const HttpResponse &response)> is_expired_response;
    std::string expired_message = "下游会话已过期，请重新登录";
    DownstreamSystemId system = DownstreamSystemId::Unknown;
};

struct AuthorizedRequestOptions {
    bool authorize = true;
    bool allow_retry = true;
    bool ensure_before_send = true;
};

/** Remote request: yes. Sends an authorized downstream request and retries only according to options. */
[[nodiscard]] Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                                           HttpRequest request,
                                                           AuthorizedRequestHooks hooks,
                                                           AuthorizedRequestOptions options);

/** Remote request: yes. Compatibility overload; session/network failures must remain explicit Result errors. */
[[nodiscard]] Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                                           HttpRequest request,
                                                           AuthorizedRequestHooks hooks,
                                                           bool authorize = true,
                                                           bool allow_retry = true);

} // namespace UBAANext::Protocol
