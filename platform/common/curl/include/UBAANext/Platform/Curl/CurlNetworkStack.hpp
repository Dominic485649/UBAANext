#pragma once

#include <UBAANext/Net/NetworkStack.hpp>
#include <UBAANext/Platform/Curl/CurlCookieStore.hpp>
#include <UBAANext/Platform/Curl/CurlHttpClient.hpp>
#include <UBAANext/Platform/Curl/CurlRedirectController.hpp>

namespace UBAANext::Platform::Curl {

class CurlNetworkStack : public UBAANext::INetworkStack {
public:
    [[nodiscard]] IHttpClient &http_client() override;
    [[nodiscard]] ICookieStore &cookie_store() override;
    [[nodiscard]] IRedirectController &redirect_controller() override;

private:
    CurlHttpClient m_http_client;
    CurlCookieStore m_cookie_store;
    CurlRedirectController m_redirect_controller;
};

} // namespace UBAANext::Platform::Curl
