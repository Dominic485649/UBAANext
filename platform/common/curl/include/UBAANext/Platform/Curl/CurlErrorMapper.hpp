#pragma once

#include <UBAANext/Base/Result.hpp>

namespace UBAANext::Platform::Curl {

[[nodiscard]] Unexpected make_curl_unavailable_error();

} // namespace UBAANext::Platform::Curl
