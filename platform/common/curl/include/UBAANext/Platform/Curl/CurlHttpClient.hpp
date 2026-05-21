#pragma once

#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Net/HttpClient.hpp>

namespace UBAANext::Platform::Curl {

class CurlHttpClient : public UBAANext::IHttpClient {
public:
    CurlHttpClient() = default;
    explicit CurlHttpClient(UBAANext::CookieJar &cookies);

    [[nodiscard]] Result<HttpResponse> send(const HttpRequest &request) override;
    [[nodiscard]] UBAANext::CookieJar &live_cookies();

private:
    UBAANext::CookieJar m_owned_cookies;
    UBAANext::CookieJar *m_live_cookies = &m_owned_cookies;
};

} // namespace UBAANext::Platform::Curl
