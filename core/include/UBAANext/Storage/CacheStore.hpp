/**
 * @file CacheStore.hpp
 * @brief 抽象缓存存储接口定义
 *
 * 提供简单的键值缓存抽象，支持可选的 TTL（存活时间）。
 * 实现可以是内存型（MemoryCacheStore）或持久型
 * （未来版本可能支持 Redis、SQLite）。
 */
#pragma once

#include <optional>
#include <string>

namespace UBAANext {

/**
 * @brief 缓存存储的抽象接口
 *
 * 用于缓存 API 响应和计算结果，
 * 以减少网络调用并提升响应速度。
 */
class ICacheStore {
public:
    virtual ~ICacheStore() = default;

    /**
     * @brief 存储一个无过期时间的值
     * @param key   缓存键
     * @param value 要缓存的值（将被移动）
     */
    virtual void set(const std::string &key, std::string value) = 0;

    /**
     * @brief 存储一个带存活时间的值
     * @param key         缓存键
     * @param value       要缓存的值（将被移动）
     * @param ttl_seconds 条目过期前的秒数（必须 > 0，否则等同于 set()）
     */
    virtual void set_with_ttl(const std::string &key, std::string value, int ttl_seconds) = 0;

    /**
     * @brief 检索已缓存的值
     * @param key 要查找的缓存键
     * @return 缓存值，如果不存在或已过期则返回 std::nullopt
     */
    [[nodiscard]] virtual std::optional<std::string> get(const std::string &key) const = 0;

    /**
     * @brief 移除一个缓存条目
     * @param key 要移除的键
     */
    virtual void remove(const std::string &key) = 0;

    /** @brief 移除所有缓存条目 */
    virtual void clear() = 0;
};

} // namespace UBAANext
