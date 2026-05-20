#pragma once

#include <UBAANext/Net/HttpResponse.hpp>

#include <string>

namespace UBAANext::Protocol {

[[nodiscard]] std::string header_value(const HttpResponse &response, const std::string &name);
[[nodiscard]] bool has_sso_login_marker(const std::string &text);
[[nodiscard]] bool has_session_expired_marker(const std::string &text);
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response);

} // namespace UBAANext::Protocol
