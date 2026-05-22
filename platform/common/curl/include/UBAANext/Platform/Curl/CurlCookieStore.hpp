#pragma once

#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <string>

namespace UBAANext::Platform::Curl {

class CurlCookieStore : public UBAANext::ICookieStore {
public:
    CurlCookieStore() = default;
    explicit CurlCookieStore(UBAANext::CookieJar &live_cookies);
    CurlCookieStore(UBAANext::CookieJar &live_cookies, UBAANext::ISecureStore &secure_store, std::string key = default_key());

    [[nodiscard]] Result<CookieJar> load() override;
    [[nodiscard]] Result<void> save(const CookieJar &cookies) override;
    [[nodiscard]] Result<void> save_current() override;
    [[nodiscard]] Result<void> clear() override;
    [[nodiscard]] const CookieJar *current() const override;

    [[nodiscard]] UBAANext::CookieJar &live_cookies();
    [[nodiscard]] const UBAANext::CookieJar &live_cookies() const;

    static constexpr const char *default_key() noexcept { return "cookies.v1"; }

private:
    UBAANext::CookieJar m_owned_cookies;
    UBAANext::CookieJar *m_live_cookies = &m_owned_cookies;
    UBAANext::ISecureStore *m_secure_store = nullptr;
    std::string m_key = default_key();
};

} // namespace UBAANext::Platform::Curl
