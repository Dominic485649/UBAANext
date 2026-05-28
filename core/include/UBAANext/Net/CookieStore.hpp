/**
 * @file CookieStore.hpp
 * @brief Cookie/session persistence boundary.
 *
 * Sensitive persistence：实现可能保存 cookie/session 标识；Unsupported/Fallback 实现必须稳定失败或明确只保存在内存中。
 */
#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/CookieJar.hpp>

namespace UBAANext {

class ICookieStore {
public:
    virtual ~ICookieStore() = default;
    /** Sensitive output: loads persisted cookies; failure must not be converted to an empty successful jar. */
    [[nodiscard]] virtual Result<CookieJar> load() = 0;
    /** Sensitive input: persists cookies/session identifiers through the platform cookie store. */
    [[nodiscard]] virtual Result<void> save(const CookieJar &cookies) = 0;
    /** Sensitive input: persists the current cookie jar, if the implementation tracks one. */
    [[nodiscard]] virtual Result<void> save_current() = 0;
    /** Sensitive session boundary: clears stored cookies without proving remote logout. */
    [[nodiscard]] virtual Result<void> clear() = 0;
    /** Sensitive output: returns the current in-memory jar when available; callers must not log it. */
    [[nodiscard]] virtual const CookieJar *current() const { return nullptr; }
};

} // namespace UBAANext
