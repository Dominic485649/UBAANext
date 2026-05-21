#pragma once

#include <UBAANext/Base/Result.hpp>

#include <curl/curl.h>

namespace UBAANext::Platform::Curl {

[[nodiscard]] Unexpected make_curl_unavailable_error();
[[nodiscard]] Unexpected map_curl_error(CURLcode code, const char *message = nullptr);

} // namespace UBAANext::Platform::Curl
