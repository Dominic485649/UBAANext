#pragma once

#include <UBAANext/Net/HttpResponse.hpp>

#include <string>

namespace UBAANext::Protocol {

[[nodiscard]] std::string header_value(const HttpResponse &response, const std::string &name);
[[nodiscard]] bool has_sso_login_marker(const std::string &text);
[[nodiscard]] bool has_session_expired_marker(const std::string &text);
[[nodiscard]] bool is_sso_or_login_url(const std::string &url);
[[nodiscard]] bool is_html_response(const std::string &body);
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response);
[[nodiscard]] bool is_session_expired_response(const HttpResponse &response, const std::string &final_url, bool expect_json = false);

} // namespace UBAANext::Protocol
