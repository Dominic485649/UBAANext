#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <string>

namespace UBAANext::Protocol::AppBuaa {

std::string resolve_url(const std::string &url, ConnectionMode mode);
void apply_ajax_headers(HttpRequest &request,
                        ConnectionMode mode,
                        const std::string &referer = "https://app.buaa.edu.cn/",
                        const std::string &user_agent = "UBAANext/0.4");
bool is_session_expired_response(const HttpResponse &response);
Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent = "UBAANext/0.4");
Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent,
                            const std::string &username,
                            const std::string &password);

} // namespace UBAANext::Protocol::AppBuaa
