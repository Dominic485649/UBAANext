#pragma once

#include <string>

namespace UBAANext {
namespace Security {

/** Sensitive output helper: redacts credentials, session/cookie/token fields, authorization headers, and local upload paths. */
[[nodiscard]] std::string redact_sensitive_text(const std::string &text);
/** Sensitive output helper: redacts proxy credentials from URLs without removing non-secret host data. */
[[nodiscard]] std::string redact_proxy_url(const std::string &proxy);

} // namespace Security
} // namespace UBAANext
