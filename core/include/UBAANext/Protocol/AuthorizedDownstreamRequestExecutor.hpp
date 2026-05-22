#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <functional>
#include <string>

namespace UBAANext::Protocol {

struct AuthorizedRequestHooks {
    std::function<Result<void>(bool force_refresh)> ensure_authorized;
    std::function<void()> invalidate;
    std::function<void(HttpRequest &request)> decorate_request;
    std::function<bool(const HttpResponse &response)> is_expired_response;
    std::string expired_message = "下游会话已过期，请重新登录";
    DownstreamSystemId system = DownstreamSystemId::Unknown;
};

struct AuthorizedRequestOptions {
    bool authorize = true;
    bool allow_retry = true;
    bool ensure_before_send = true;
};

[[nodiscard]] Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                                           HttpRequest request,
                                                           AuthorizedRequestHooks hooks,
                                                           AuthorizedRequestOptions options);

[[nodiscard]] Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                                           HttpRequest request,
                                                           AuthorizedRequestHooks hooks,
                                                           bool authorize = true,
                                                           bool allow_retry = true);

} // namespace UBAANext::Protocol
