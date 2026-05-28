/**
 * @file SessionManager.hpp
 * @brief 基于 ISecureStore 的持久会话管理
 *
 * 处理登录会话的保存、恢复和清除，
 * 使其在应用重启后仍然存在。
 * 使用 ISecureStore 进行持久化，确保令牌在进程终止后存活。
 */
#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <optional>
#include <string>

namespace UBAANext {

/**
 * @brief 通过 ISecureStore 管理持久登录会话
 *
 * Sensitive session persistence：save/restore/clear 会读写 username、account、connection_mode 等 session 字段。
 * Unsupported/Fallback store 不能被解释为真实 secure-store 能力完成。
 */
class SessionManager {
public:
    /**
     * @brief 构造由给定存储支撑的 SessionManager
     * @param store 底层安全存储（非拥有）
     */
    explicit SessionManager(ISecureStore &store);

    /**
     * @brief Sensitive input：将登录会话保存到 secure store 或其 fallback 实现中。
     * @param username 用于登录的学号，不得输出到日志或 diagnostics
     * @param account  要持久化的账户数据（复制到存储，移动到缓存）
     */
    Result<void> save_session(const std::string &username, Model::Account account, const std::string &connection_mode = "");

    /**
     * @brief Sensitive output：从存储中恢复之前保存的会话。
     * @return 恢复的账户对象，如果没有有效会话则返回 std::nullopt
     */
    [[nodiscard]] std::optional<Model::Account> restore_session();

    /** @brief Sensitive session boundary：从存储和内存缓存中清除会话 */
    void clear_session();

    /**
     * @brief Local session state only：不证明远端 session 仍有效。
     * @return 如果 restore_session() 或 save_session() 已被成功调用则返回 true
     */
    [[nodiscard]] bool has_session() const;

    /**
     * @brief Sensitive output：获取当前会话关联的用户名，不得直接写入日志。
     * @return 用户名字符串，无活跃会话时返回空字符串
     */
    [[nodiscard]] const std::string &current_username() const;
    /** Sensitive session boundary: restored connection mode is local state only. */
    [[nodiscard]] const std::string &connection_mode() const;

private:
    ISecureStore &m_store;                    ///< Sensitive persistence backend（可能是 Unsupported/Fallback）
    std::optional<Model::Account> m_current;  ///< Sensitive session/account cache
    std::string m_username;                   ///< Sensitive username, do not log
    std::string m_connection_mode;            ///< Local routing mode, not live capability proof
};

} // namespace UBAANext
