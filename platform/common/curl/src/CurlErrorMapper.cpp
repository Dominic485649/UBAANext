#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>

#include <string>

namespace UBAANext::Platform::Curl {

Unexpected make_curl_unavailable_error() {
    return make_error(ErrorCode::UnsupportedNetwork, "Curl network adapter is not available in this build");
}

Unexpected map_curl_error(CURLcode code, const char *message) {
    const std::string detail = message && *message ? message : curl_easy_strerror(code);
    switch (code) {
    case CURLE_OPERATION_TIMEDOUT:
        return make_error(ErrorCode::Timeout, "Curl 请求超时: " + detail);
    case CURLE_SSL_CONNECT_ERROR:
    case CURLE_SSL_ENGINE_NOTFOUND:
    case CURLE_SSL_ENGINE_SETFAILED:
    case CURLE_SSL_CERTPROBLEM:
    case CURLE_SSL_CIPHER:
    case CURLE_PEER_FAILED_VERIFICATION:
    case CURLE_SSL_ENGINE_INITFAILED:
    case CURLE_SSL_CACERT_BADFILE:
    case CURLE_SSL_SHUTDOWN_FAILED:
    case CURLE_SSL_CRL_BADFILE:
    case CURLE_SSL_ISSUER_ERROR:
        return make_error(ErrorCode::TlsError, "Curl TLS 失败: " + detail);
    case CURLE_UNSUPPORTED_PROTOCOL:
    case CURLE_NOT_BUILT_IN:
        return make_error(ErrorCode::UnsupportedNetwork, "Curl 不支持该网络能力: " + detail);
    default:
        return make_error(ErrorCode::NetworkError, "Curl 请求失败: " + detail);
    }
}

} // namespace UBAANext::Platform::Curl
