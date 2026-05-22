#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <functional>
#include <string>

namespace UBAANext::Protocol {

struct RedirectStep {
    std::string request_url;
    std::string resolved_url;
    HttpResponse response;
};

struct RedirectNavigationResult {
    std::string final_url;
    HttpResponse final_response;
};

using RedirectStepObserver = std::function<Result<void>(const RedirectStep &step)>;
using UrlResolver = std::function<std::string(const std::string &url, ConnectionMode mode)>;
using RequestConfigurer = std::function<void(HttpRequest &request)>;

[[nodiscard]] std::string resolve_location(const std::string &base_url, const std::string &location);
[[nodiscard]] std::string extract_query_parameter(const std::string &url, const std::string &name);
[[nodiscard]] std::string extract_query_parameter_anywhere(const std::string &url, const std::string &name);
[[nodiscard]] std::string redact_url_query(const std::string &url);

void disable_transport_redirects(HttpRequest &request);

[[nodiscard]] Result<RedirectNavigationResult> follow_redirects(IHttpClient &http_client,
                                                                ConnectionMode mode,
                                                                std::string initial_url,
                                                                DownstreamSystemId system,
                                                                UrlResolver resolve_url,
                                                                RequestConfigurer configure_request = {},
                                                                RedirectStepObserver observe_step = {},
                                                                int max_redirects = 12);

} // namespace UBAANext::Protocol
