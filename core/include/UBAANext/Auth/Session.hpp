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
 * Sensitive session boundary：仅保存进程内 Account，不负责持久化。
 * 返回的账户字段可能包含身份/session 信息，必须走 redaction-aware 输出路径。
 */
class Session {
public:
    Session() = default;

    /**
     * @brief Sensitive input：设置此会话的活跃账户，可能包含 token/session 字段。
     * @param account 账户数据（将被移动到会话中）
     */
    void set_account(Model::Account account);

    /**
     * @brief Sensitive output：获取当前账户（如有），调用方不得直接写入日志。
     * @return 指向可选账户的常量引用
     */
    [[nodiscard]] const std::optional<Model::Account> &account() const;

    /**
     * @brief Local session state only：不证明远端 session 仍有效。
     * @return 如果已设置账户则返回 true
     */
    [[nodiscard]] bool is_valid() const;

    /** @brief 清除内存会话，不证明远端 logout 已执行 */
    void clear();

private:
    std::optional<Model::Account> m_account;  ///< Sensitive session/account cache（仅内存）
};

} // namespace UBAANext
