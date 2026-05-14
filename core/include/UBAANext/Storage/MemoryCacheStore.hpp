/**
 * @file MemoryCacheStore.hpp
 * @brief 带可选 TTL 的内存缓存实现
 *
 * 使用 std::unordered_map 实现 ICacheStore，
 * 配合 std::chrono::steady_clock 进行单调过期追踪。
 * 过期条目在读取时被惰性清除。
 */
#pragma once

#include <UBAANext/Storage/CacheStore.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace UBAANext {

/**
 * @brief 带逐条目 TTL 的内存缓存存储
 *
 * 线程安全：非线程安全。
 * 如果从多线程使用，调用方必须自行同步。
 */
class MemoryCacheStore : public ICacheStore {
public:
    /// @copydoc ICacheStore::set
    void set(const std::string &key, std::string value) override;

    /// @copydoc ICacheStore::set_with_ttl
    void set_with_ttl(const std::string &key, std::string value, int ttl_seconds) override;

    /// @copydoc ICacheStore::get
    [[nodiscard]] std::optional<std::string> get(const std::string &key) const override;

    /// @copydoc ICacheStore::remove
    void remove(const std::string &key) override;

    /// @copydoc ICacheStore::clear
    void clear() override;

private:
    /** @brief 单个缓存条目，包含可选的过期时间 */
    struct CacheEntry {
        std::string value;                                  ///< 缓存的数据
        std::chrono::steady_clock::time_point expires_at;   ///< 过期时间戳（仅当 has_ttl 为 true 时有效）
        bool has_ttl = false;                               ///< 此条目是否有 TTL
    };

    /// @brief 底层存储 map。声明为 mutable 以允许在 const get() 中进行惰性清除。
    mutable std::unordered_map<std::string, CacheEntry> m_data;
};

} // namespace UBAANext
