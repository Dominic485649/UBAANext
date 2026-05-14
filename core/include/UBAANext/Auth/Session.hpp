/**
 * @file Session.hpp
 * @brief 当前登录的内存会话状态
 *
 * 持有当前活跃的 Account 对象（如有）。
 * 不处理持久化——那是 SessionManager 的职责。
 */
#pragma once

#include <UBAANext/Model/Account.hpp>

#include <optional>
#include <string>

namespace UBAANext {

/**
 * @brief 表示当前活跃的内存登录会话
 *
 * 当 Session 持有 Account（通过 set_account 设置）时会话有效，
 * 通过 clear() 清除。此类非线程安全。
 */
class Session {
public:
    Session() = default;

    /**
     * @brief 设置此会话的活跃账户
     * @param account 账户数据（将被移动到会话中）
     */
    void set_account(Model::Account account);

    /**
     * @brief 获取当前账户（如有）
     * @return 指向可选账户的常量引用
     */
    [[nodiscard]] const std::optional<Model::Account> &account() const;

    /**
     * @brief 检查会话是否持有活跃账户
     * @return 如果已设置账户则返回 true
     */
    [[nodiscard]] bool is_valid() const;

    /** @brief 清除会话，移除已存储的账户 */
    void clear();

private:
    std::optional<Model::Account> m_account;  ///< 活跃账户（已登录时有值）
};

} // namespace UBAANext
