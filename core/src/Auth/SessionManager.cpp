/**
 * @file SessionManager.cpp
 * @brief 持久会话管理的实现
 *
 * 存储在 ISecureStore 中的会话键：
 *   - session.username      用于登录的学号
 *   - session.student_id    账户的 student_id 字段
 *   - session.display_name  账户的 display_name 字段
 *   - session.access_token  JWT 访问令牌
 *   - session.refresh_token JWT 刷新令牌
 *   - session.active        "true" 表示会话已保存
 */

#include <UBAANext/Auth/SessionManager.hpp>

namespace UBAANext {

SessionManager::SessionManager(ISecureStore &store) : m_store(store) {}

Result<void> SessionManager::save_session(const std::string &username,
                                          Model::Account account,
                                          const std::string &connection_mode) {
    // 输入验证：用户名不能为空
    if (username.empty()) {
        return make_error(ErrorCode::InvalidArgument, "保存会话失败: 用户名不能为空");
    }

    // 更新内存缓存（使用移动语义避免不必要的拷贝）
    m_username = username;
    m_connection_mode = connection_mode;
    m_current = account;

    // 持久化到安全存储
    m_store.set_string("session.username", username);
    m_store.set_string("session.student_id", account.student_id);
    m_store.set_string("session.display_name", account.display_name);
    m_store.set_string("session.access_token", account.access_token);
    m_store.set_string("session.refresh_token", account.refresh_token);
    if (!connection_mode.empty()) {
        m_store.set_string("session.connection_mode", connection_mode);
    } else {
        m_store.remove("session.connection_mode");
    }
    m_store.set_string("session.active", "true");
    auto flushed = m_store.flush();
    if (!flushed) {
        return make_error(flushed.error().code, flushed.error().message);
    }
    return {};
}

std::optional<Model::Account> SessionManager::restore_session() {
    // 检查之前是否保存过会话
    auto active = m_store.get_string("session.active");
    if (!active || *active != "true") {
        return std::nullopt;
    }

    // 读取必需字段
    auto username = m_store.get_string("session.username");
    auto student_id = m_store.get_string("session.student_id");
    auto display_name = m_store.get_string("session.display_name");
    auto connection_mode = m_store.get_string("session.connection_mode");

    // 如果任何必需字段缺失，则会话无效
    if (!username || !student_id || !display_name) {
        return std::nullopt;
    }

    Model::Account account;
    account.student_id = std::move(*student_id);
    account.display_name = std::move(*display_name);

    // 读取可选的令牌字段
    auto access_token = m_store.get_string("session.access_token");
    auto refresh_token = m_store.get_string("session.refresh_token");
    if (access_token) account.access_token = std::move(*access_token);
    if (refresh_token) account.refresh_token = std::move(*refresh_token);

    // 更新内存缓存
    m_username = std::move(*username);
    m_connection_mode = connection_mode ? std::move(*connection_mode) : std::string{};
    m_current = account;
    return account;
}

void SessionManager::clear_session() {
    // 从存储中移除所有会话键
    m_store.remove("session.username");
    m_store.remove("session.student_id");
    m_store.remove("session.display_name");
    m_store.remove("session.access_token");
    m_store.remove("session.refresh_token");
    m_store.remove("session.connection_mode");
    m_store.remove("session.active");
    (void)m_store.flush();

    // 清除内存缓存
    m_current.reset();
    m_username.clear();
    m_connection_mode.clear();
}

bool SessionManager::has_session() const {
    return m_current.has_value();
}

const std::string &SessionManager::current_username() const {
    return m_username;
}

const std::string &SessionManager::connection_mode() const {
    return m_connection_mode;
}

} // namespace UBAANext
