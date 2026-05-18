#pragma once

#include <UBAANext/Base/Result.hpp>

namespace UBAANext::Platform::OpenSSL {

[[nodiscard]] Unexpected make_openssl_unavailable_error();

} // namespace UBAANext::Platform::OpenSSL
