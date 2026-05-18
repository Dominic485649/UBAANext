#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>

namespace UBAANext::Platform::Curl {

IHttpClient &CurlNetworkStack::http_client() {
    return m_http_client;
}

ICookieStore &CurlNetworkStack::cookie_store() {
    return m_cookie_store;
}

IRedirectController &CurlNetworkStack::redirect_controller() {
    return m_redirect_controller;
}

} // namespace UBAANext::Platform::Curl
