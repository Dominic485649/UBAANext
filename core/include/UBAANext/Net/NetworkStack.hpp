#pragma once

#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/RedirectController.hpp>

namespace UBAANext {

class INetworkStack {
public:
    virtual ~INetworkStack() = default;
    [[nodiscard]] virtual IHttpClient &http_client() = 0;
    [[nodiscard]] virtual ICookieStore &cookie_store() = 0;
    [[nodiscard]] virtual IRedirectController &redirect_controller() = 0;
};

} // namespace UBAANext
