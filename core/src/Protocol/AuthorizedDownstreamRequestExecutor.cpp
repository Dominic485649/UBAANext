#include <UBAANext/Protocol/AuthorizedDownstreamRequestExecutor.hpp>

namespace UBAANext::Protocol {

Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                             HttpRequest request,
                                             AuthorizedRequestHooks hooks,
                                             AuthorizedRequestOptions options) {
    if (options.authorize && options.ensure_before_send && hooks.ensure_authorized) {
        auto ready = hooks.ensure_authorized(false);
        if (!ready) {
            return make_error(ready.error().code, ready.error().message);
        }
    }

    if (options.authorize && hooks.decorate_request) {
        hooks.decorate_request(request);
    }

    auto response = http_client.send(request);
    if (!response) {
        return make_error(response.error().code, response.error().message);
    }

    if (options.authorize && hooks.is_expired_response && hooks.is_expired_response(*response)) {
        if (hooks.invalidate) {
            hooks.invalidate();
        }
        if (options.allow_retry && hooks.ensure_authorized) {
            auto ready = hooks.ensure_authorized(true);
            if (!ready) {
                return make_error(ready.error().code, ready.error().message);
            }
            AuthorizedRequestOptions retry_options;
            retry_options.authorize = options.authorize;
            retry_options.allow_retry = false;
            retry_options.ensure_before_send = false;
            return send_authorized_request(http_client, std::move(request), std::move(hooks), retry_options);
        }

        auto error = make_downstream_error(hooks.system,
                                           options.allow_retry ? DownstreamActivationStage::RetryAfterRefresh : DownstreamActivationStage::Request,
                                           DownstreamSessionState::TokenExpired,
                                           hooks.expired_message.empty() ? "下游会话已过期，请重新登录" : hooks.expired_message);
        return make_error(error.code, to_error(error).message);
    }

    return *response;
}

Result<HttpResponse> send_authorized_request(IHttpClient &http_client,
                                             HttpRequest request,
                                             AuthorizedRequestHooks hooks,
                                             bool authorize,
                                             bool allow_retry) {
    AuthorizedRequestOptions options;
    options.authorize = authorize;
    options.allow_retry = allow_retry;
    return send_authorized_request(http_client, std::move(request), std::move(hooks), options);
}

} // namespace UBAANext::Protocol
