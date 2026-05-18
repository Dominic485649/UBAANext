#include <UBAANext/Platform/Curl/CurlCookieStore.hpp>

#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>

namespace UBAANext::Platform::Curl {

Result<CookieJar> CurlCookieStore::load() {
    return make_curl_unavailable_error();
}

Result<void> CurlCookieStore::save(const CookieJar &cookies) {
    (void)cookies;
    return make_curl_unavailable_error();
}

Result<void> CurlCookieStore::clear() {
    return make_curl_unavailable_error();
}

} // namespace UBAANext::Platform::Curl
