#include "SecurityRedaction.hpp"

#include <UBAANext/Security/SecurityRedaction.hpp>

namespace UBAANextCli {

std::string redact_proxy_url(const std::string &proxy) {
    return UBAANext::Security::redact_proxy_url(proxy);
}

std::string redact_sensitive_text(const std::string &text) {
    return UBAANext::Security::redact_sensitive_text(text);
}

} // namespace UBAANextCli
