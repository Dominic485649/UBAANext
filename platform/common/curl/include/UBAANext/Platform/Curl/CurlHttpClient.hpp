#pragma once

#include <UBAANext/Net/HttpClient.hpp>

namespace UBAANext::Platform::Curl {

class CurlHttpClient : public UBAANext::IHttpClient {
public:
    [[nodiscard]] Result<HttpResponse> send(const HttpRequest &request) override;
};

} // namespace UBAANext::Platform::Curl
