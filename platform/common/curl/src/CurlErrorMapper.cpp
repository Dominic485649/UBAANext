#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>

namespace UBAANext::Platform::Curl {

Unexpected make_curl_unavailable_error() {
    return make_error(ErrorCode::NetworkError, "Curl network adapter is not available in this build");
}

} // namespace UBAANext::Platform::Curl
