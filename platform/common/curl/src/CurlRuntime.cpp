#include <UBAANext/Platform/Curl/CurlRuntime.hpp>

#include <curl/curl.h>

#include <mutex>

namespace UBAANext::Platform::Curl {
namespace {

std::once_flag g_curl_init_once;
CURLcode g_curl_init_code = CURLE_FAILED_INIT;

} // namespace

CurlRuntime::CurlRuntime() {
    std::call_once(g_curl_init_once, [] {
        g_curl_init_code = curl_global_init(CURL_GLOBAL_DEFAULT);
    });
    if (g_curl_init_code == CURLE_OK) {
        m_ok = true;
        return;
    }
    m_error = Error(ErrorCode::UnsupportedNetwork, std::string("Curl 全局初始化失败: ") + curl_easy_strerror(g_curl_init_code));
}

CurlRuntime::~CurlRuntime() = default;

} // namespace UBAANext::Platform::Curl
