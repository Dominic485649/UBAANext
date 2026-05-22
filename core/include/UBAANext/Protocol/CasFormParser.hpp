#pragma once

#include <string>

namespace UBAANext::Protocol {

[[nodiscard]] std::string form_url_encode(const std::string &value);
[[nodiscard]] std::string extract_execution(const std::string &html);
[[nodiscard]] std::string build_login_form(const std::string &html,
                                           const std::string &username,
                                           const std::string &password,
                                           const std::string &execution,
                                           const std::string &captcha = "");

} // namespace UBAANext::Protocol
