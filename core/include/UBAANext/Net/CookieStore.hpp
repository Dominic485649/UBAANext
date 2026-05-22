#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/CookieJar.hpp>

namespace UBAANext {

class ICookieStore {
public:
    virtual ~ICookieStore() = default;
    [[nodiscard]] virtual Result<CookieJar> load() = 0;
    [[nodiscard]] virtual Result<void> save(const CookieJar &cookies) = 0;
    [[nodiscard]] virtual Result<void> save_current() = 0;
    [[nodiscard]] virtual Result<void> clear() = 0;
    [[nodiscard]] virtual const CookieJar *current() const { return nullptr; }
};

} // namespace UBAANext
