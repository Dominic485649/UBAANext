/**
 * @file CasFormParser.hpp
 * @brief CAS login form parsing/building helpers.
 *
 * Sensitive input/output：helpers process login HTML, execution tokens, credentials, and captcha; none may be logged verbatim.
 */
#pragma once

#include <string>

namespace UBAANext::Protocol {

/** Sensitive input: URL-encodes form values that may contain credentials or captcha. */
[[nodiscard]] std::string form_url_encode(const std::string &value);
/** PartiallyMigrated parser entry: extracts CAS execution token from raw login HTML. */
[[nodiscard]] std::string extract_execution(const std::string &html);
/** Sensitive input: builds CAS login form containing username/password/captcha and must not be logged. */
[[nodiscard]] std::string build_login_form(const std::string &html,
                                           const std::string &username,
                                           const std::string &password,
                                           const std::string &execution,
                                           const std::string &captcha = "");

} // namespace UBAANext::Protocol
