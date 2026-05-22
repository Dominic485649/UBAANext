#include <UBAANext/Platform/Curl/CurlCookieStore.hpp>

#include <sstream>

namespace UBAANext::Platform::Curl {
namespace {

constexpr const char *kCookieBlobPrefix = "UBAANext-Cookies-v1\n";

std::string serialize_cookie_blob(const CookieJar &cookies) {
    std::string blob = kCookieBlobPrefix;
    for (const auto &line : cookies.serialize()) {
        blob += line;
        blob += '\n';
    }
    return blob;
}

Result<CookieJar> parse_cookie_blob(const std::string &blob) {
    if (blob.rfind(kCookieBlobPrefix, 0) != 0) {
        return make_error(ErrorCode::StorageError, "Cookie 存储格式不受支持");
    }

    CookieJar cookies;
    std::istringstream lines(blob.substr(std::string(kCookieBlobPrefix).size()));
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            cookies.load_serialized_line(line);
        }
    }
    return cookies;
}

} // namespace

CurlCookieStore::CurlCookieStore(UBAANext::CookieJar &live_cookies)
    : m_live_cookies(&live_cookies) {}

CurlCookieStore::CurlCookieStore(UBAANext::CookieJar &live_cookies, UBAANext::ISecureStore &secure_store, std::string key)
    : m_live_cookies(&live_cookies), m_secure_store(&secure_store), m_key(std::move(key)) {}

Result<CookieJar> CurlCookieStore::load() {
    if (!m_secure_store) {
        return make_error(ErrorCode::UnsupportedCookiePersistence, "当前平台尚未接入安全 Cookie 持久化");
    }

    auto blob = m_secure_store->get_string(m_key);
    if (!blob) {
        m_live_cookies->clear();
        return *m_live_cookies;
    }

    auto parsed = parse_cookie_blob(*blob);
    if (!parsed) {
        return make_error(parsed.error().code, parsed.error().message);
    }
    *m_live_cookies = std::move(*parsed);
    return *m_live_cookies;
}

Result<void> CurlCookieStore::save(const CookieJar &cookies) {
    if (!m_secure_store) {
        return make_error(ErrorCode::UnsupportedCookiePersistence, "当前平台尚未接入安全 Cookie 持久化");
    }

    *m_live_cookies = cookies;
    return save_current();
}

Result<void> CurlCookieStore::save_current() {
    if (!m_secure_store) {
        return make_error(ErrorCode::UnsupportedCookiePersistence, "当前平台尚未接入安全 Cookie 持久化");
    }

    m_secure_store->set_string(m_key, serialize_cookie_blob(*m_live_cookies));
    auto flushed = m_secure_store->flush();
    if (!flushed) {
        return make_error(flushed.error().code, flushed.error().message);
    }
    return {};
}

Result<void> CurlCookieStore::clear() {
    m_live_cookies->clear();
    if (!m_secure_store) {
        return make_error(ErrorCode::UnsupportedCookiePersistence, "当前平台尚未接入安全 Cookie 持久化");
    }

    m_secure_store->remove(m_key);
    auto flushed = m_secure_store->flush();
    if (!flushed) {
        return make_error(flushed.error().code, flushed.error().message);
    }
    return {};
}

const UBAANext::CookieJar *CurlCookieStore::current() const {
    return m_live_cookies;
}

UBAANext::CookieJar &CurlCookieStore::live_cookies() {
    return *m_live_cookies;
}

const UBAANext::CookieJar &CurlCookieStore::live_cookies() const {
    return *m_live_cookies;
}

} // namespace UBAANext::Platform::Curl
