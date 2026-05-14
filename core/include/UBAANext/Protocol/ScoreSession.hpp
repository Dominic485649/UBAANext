#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <string>

namespace UBAANext::Protocol::Score {

std::string resolve_url(const std::string &url, ConnectionMode mode);
void apply_form_headers(HttpRequest &request);
bool is_session_expired_response(const HttpResponse &response);
Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode);

} // namespace UBAANext::Protocol::Score
