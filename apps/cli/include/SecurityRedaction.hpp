#pragma once

#include <string>

namespace UBAANextCli {

[[nodiscard]] std::string redact_sensitive_text(const std::string &text);
[[nodiscard]] std::string redact_proxy_url(const std::string &proxy);

} // namespace UBAANextCli
