#include <UBAANext/Platform/OpenSSL/OpenSslErrorMapper.hpp>

namespace UBAANext::Platform::OpenSSL {

Unexpected make_openssl_unavailable_error() {
    return make_error(ErrorCode::UnsupportedCrypto, "OpenSSL crypto adapter is not available in this build");
}

} // namespace UBAANext::Platform::OpenSSL
