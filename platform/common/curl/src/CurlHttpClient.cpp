#include <UBAANext/Platform/Curl/CurlHttpClient.hpp>

#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>

namespace UBAANext::Platform::Curl {

Result<HttpResponse> CurlHttpClient::send(const HttpRequest &request) {
    (void)request;
    return make_curl_unavailable_error();
}

} // namespace UBAANext::Platform::Curl
