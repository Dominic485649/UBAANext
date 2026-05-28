/**
 * @file NetworkStack.hpp
 * @brief Network, cookie, and redirect adapter boundary.
 *
 * Unsupported/Fallback implementations must not be interpreted as live network, cookie persistence, or redirect coverage.
 */
#pragma once

#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/RedirectController.hpp>

namespace UBAANext {

class INetworkStack {
public:
    virtual ~INetworkStack() = default;
    /** Sensitive transport boundary: may send real remote requests depending on implementation. */
    [[nodiscard]] virtual IHttpClient &http_client() = 0;
    /** Sensitive persistence boundary: may persist cookie/session identifiers depending on implementation. */
    [[nodiscard]] virtual ICookieStore &cookie_store() = 0;
    /** PartiallyMigrated redirect boundary: controls session navigation, not business completion. */
    [[nodiscard]] virtual IRedirectController &redirect_controller() = 0;
};

} // namespace UBAANext
