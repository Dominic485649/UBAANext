/**
 * @file SessionManager.hpp
 * @brief 基于 ISecureStore 的持久会话管理
 *
 * 处理登录会话的保存、恢复和清除，
 * 使其在应用重启后仍然存在。
 * 使用 ISecureStore 进行持久化，确保令牌在进程终止后存活。
 */
#pragma once

#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <optional>
#include <string>

namespace UBAANext {

/**
 * @brief 通过 ISecureStore 管理持久登录会话
 *
 * 在 save_session() 时，账户各字段以 "session.*" 键写入存储。
 * 在 restore_session() 时，读回这些字段并组装为 Account 返回。
 * clear_session() 移除所有会话键。
 *
 * SessionManager 还维护一个内存中的当前会话缓存，
 * 用于快速的 has_session() 检查。
 */
class SessionManager {
public:
    /**
     * @brief 构造由给定存储支撑的 SessionManager
     * @param store 底层安全存储（非拥有）
     */
    explicit SessionManager(ISecureStore &store);

    /**
     * @brief 将登录会话保存到存储中
     * @param username 用于登录的学号
     * @param account  要持久化的账户数据（复制到存储，移动到缓存）
     */
    void save_session(const std::string &username, Model::Account account, const std::string &connection_mode = "");

    /**
     * @brief 从存储中恢复之前保存的会话
     * @return 恢复的账户对象，如果没有有效会话则返回 std::nullopt
     */
    [[nodiscard]] std::optional<Model::Account> restore_session();

    /** @brief 从存储和内存缓存中清除会话 */
    void clear_session();

    /**
     * @brief 检查内存中是否有活跃会话
     * @return 如果 restore_session() 或 save_session() 已被成功调用则返回 true
     */
    [[nodiscard]] bool has_session() const;

    /**
     * @brief 获取当前会话关联的用户名
     * @return 用户名字符串，无活跃会话时返回空字符串
     */
    [[nodiscard]] const std::string &current_username() const;
    [[nodiscard]] const std::string &connection_mode() const;

private:
    ISecureStore &m_store;                    ///< 持久存储（非拥有）
    std::optional<Model::Account> m_current;  ///< 内存中的会话缓存
    std::string m_username;                   ///< 当前会话的用户名
    std::string m_connection_mode;            ///< 当前会话的连接模式
};

} // namespace UBAANext
