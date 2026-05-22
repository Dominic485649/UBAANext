/**
 * @file SecureStore.hpp
 * @brief 抽象安全键值存储接口定义
 *
 * 实现类提供平台特定的安全存储
 * （例如 Windows DPAPI、macOS Keychain）。
 * Mock 实现使用内存 map，仅用于测试。
 */
#pragma once

#include <UBAANext/Base/Result.hpp>

#include <optional>
#include <string>

namespace UBAANext {

/**
 * @brief 抽象安全键值存储接口
 *
 * 用于持久化敏感数据（如认证令牌）。
 * 所有值以字符串存储；序列化由调用方负责。
 */
class ISecureStore {
public:
    virtual ~ISecureStore() = default;

    /**
     * @brief 在给定键下存储字符串值
     * @param key   存储键（例如 "session.access_token"）
     * @param value 要存储的值
     */
    virtual void set_string(const std::string &key, const std::string &value) = 0;

    /**
     * @brief 检索已存储的字符串值
     * @param key 要查找的存储键
     * @return 已存储的值，如果键不存在则返回 std::nullopt
     */
    [[nodiscard]] virtual std::optional<std::string> get_string(const std::string &key) const = 0;

    /**
     * @brief 移除已存储的键值对
     * @param key 要移除的键
     */
    virtual void remove(const std::string &key) = 0;

    /**
     * @brief Flush pending secure-store updates to durable storage when supported.
     *
     * Implementations with immediate persistence can return success. File-backed
     * implementations should override this so multi-process CLI flows can observe
     * credentials/cookies saved by a previous command without waiting for object
     * destruction timing.
     */
    virtual Result<void> flush() { return {}; }

    /** @brief 移除所有已存储的键值对 */
    virtual void clear() = 0;
};

} // namespace UBAANext
