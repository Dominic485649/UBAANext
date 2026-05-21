#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>

namespace UBAANext::Platform::Curl {

CurlNetworkStack::CurlNetworkStack()
    : m_http_client(m_cookies), m_cookie_store(m_cookies) {}

CurlNetworkStack::CurlNetworkStack(UBAANext::ISecureStore &secure_store)
    : m_http_client(m_cookies), m_cookie_store(m_cookies, secure_store) {}

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
