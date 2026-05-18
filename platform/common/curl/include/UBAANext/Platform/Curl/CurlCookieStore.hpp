#pragma once

#include <UBAANext/Net/CookieStore.hpp>

namespace UBAANext::Platform::Curl {

class CurlCookieStore : public UBAANext::ICookieStore {
public:
    [[nodiscard]] Result<CookieJar> load() override;
    [[nodiscard]] Result<void> save(const CookieJar &cookies) override;
    [[nodiscard]] Result<void> clear() override;
};

} // namespace UBAANext::Platform::Curl
