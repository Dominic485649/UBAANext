/**
 * @file MemoryCacheStore.cpp
 * @brief 带 TTL 支持的内存缓存实现
 *
 * 使用聚合初始化构造 CacheEntry，
 * 使用 std::chrono::steady_clock 进行单调时间追踪。
 */

#include <UBAANext/Storage/MemoryCacheStore.hpp>

namespace UBAANext {

void MemoryCacheStore::set(const std::string &key, std::string value) {
    // 无 TTL 的缓存条目：has_ttl 设为 false
    m_data[key] = CacheEntry{std::move(value), {}, false};
}

void MemoryCacheStore::set_with_ttl(const std::string &key,
                                     std::string value,
                                     int ttl_seconds) {
    // 如果 TTL <= 0，退化为无过期的普通存储
    if (ttl_seconds <= 0) {
        m_data[key] = CacheEntry{std::move(value), {}, false};
        return;
    }

    // 有 TTL 的缓存条目：计算过期时间点
    m_data[key] = CacheEntry{
        std::move(value),
        std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds),
        true
    };
}

std::optional<std::string> MemoryCacheStore::get(const std::string &key) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) {
        return std::nullopt;
    }

    // 惰性清除：访问时擦除已过期的条目。
    // 注意：此处通过 mutable 修饰的 m_data 在 const 方法中执行修改，
    // 这是惰性清除模式的惯用做法。逻辑上"读取过期数据 = 删除并返回空"，
    // 对调用者而言仍满足 const 语义（不改变有效数据的可见状态）。
    // 但此操作非线程安全——多线程并发调用 get() 需外部同步。
    if (it->second.has_ttl &&
        std::chrono::steady_clock::now() > it->second.expires_at) {
        m_data.erase(it);
        return std::nullopt;
    }

    return it->second.value;
}

void MemoryCacheStore::remove(const std::string &key) {
    m_data.erase(key);
}

void MemoryCacheStore::clear() {
    m_data.clear();
}

} // namespace UBAANext