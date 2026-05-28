/**
 * @file SecureStoreAdapter.hpp
 * @brief ISecureStore 之上的高级 Account 持久化适配器
 *
 * 提供专门针对 Model::Account 的保存/加载/清除操作，
 * 将 Account 的各字段序列化到 ISecureStore 的各个键中。
 */
#pragma once

#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <optional>

namespace UBAANext {

/**
 * @brief 将 Account 对象序列化/反序列化到 ISecureStore 的适配器
 *
 * 使用的键前缀："account.student_id"、"account.display_name"、
 *               "account.access_token"、"account.refresh_token"。
 *
 * @note 此适配器是通用的 Account 持久化工具，独立于 SessionManager
 *       （后者使用 "session.*" 前缀）。AuthService 目前使用
 *       SessionManager 进行会话持久化，SecureStoreAdapter 可用于
 *       其他需要单独保存/加载 Account 的场景。
 */
class SecureStoreAdapter {
public:
    /**
     * @brief 构造适配器，包装给定的存储
     * @param store 底层安全存储（非拥有）
     */
    explicit SecureStoreAdapter(ISecureStore &store);

    /**
     * @brief 将 Account 的字段持久化到存储中
     * @param account 要保存的账户对象
     * @note Sensitive input: account tokens require a real secure-store implementation, not volatile fallback storage.
     */
    void save_account(const Model::Account &account);

    /**
     * @brief 从存储中加载 Account
     * @return 反序列化后的账户对象，如果存储中无账户则返回 std::nullopt
     * @note Sensitive output: loaded account fields must remain redaction-aware.
     */
    [[nodiscard]] std::optional<Model::Account> load_account() const;

    /** @brief 从存储中移除所有账户相关的键 */
    void clear_account();

private:
    ISecureStore &m_store;  ///< 底层存储（非拥有）
};

} // namespace UBAANext
