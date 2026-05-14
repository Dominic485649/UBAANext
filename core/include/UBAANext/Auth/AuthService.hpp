/**
 * @file AuthService.hpp
 * @brief 认证服务门面（Facade）
 *
 * 提供登录、登出、会话恢复和会话查询操作。
 * 协调 IHttpClient（用于 API 调用）、ISecureStore（用于持久化）
 * 和 SessionManager（用于会话生命周期）之间的交互。
 */

#pragma once

#include <UBAANext/Auth/Session.hpp>
#include <UBAANext/Auth/SessionManager.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <string>

namespace UBAANext {

/**
 * @brief 连接模式
 */
enum class ConnectionMode {
    Mock,       ///< 模拟数据
    Direct,     ///< 直连内网
    WebVPN,     ///< 通过 WebVPN 网关
};

/**
 * @brief 高级认证服务
 *
 * 编排登录/登出流程：
 * - login_mock():       模拟登录
 * - login_real():       真实 CAS 登录（支持内网/VPN）
 * - logout():           从存储和内存中清除会话
 * - restore_session():  从持久存储中重新恢复会话
 * - has_session() / session(): 查询当前会话状态
 */
class AuthService {
public:
    AuthService(IHttpClient &http_client, ISecureStore &secure_store);

    /**
     * @brief 执行模拟登录（无网络调用）
     */
    Result<Model::Account> login_mock(const std::string &username,
                                      const std::string &password);

    /**
     * @brief 执行真实 CAS 登录
     * @param username 学号
     * @param password 密码
     * @param mode     连接模式（Direct 或 WebVPN）
     * @param captcha  验证码（可选，如果 CAS 要求）
     * @return 登录成功后的账户信息
     */
    Result<Model::Account> login_real(const std::string &username,
                                      const std::string &password,
                                      ConnectionMode mode,
                                      const std::string &captcha = "");

    Result<void> logout();
    [[nodiscard]] bool has_session() const;
    [[nodiscard]] const Session &session() const;
    [[nodiscard]] SessionManager &session_manager();
    Result<Model::Account> restore_session();

    void set_connection_mode(ConnectionMode mode) { m_conn_mode = mode; }
    [[nodiscard]] ConnectionMode connection_mode() const { return m_conn_mode; }

    /**
     * @brief 将 URL 转换为对应连接模式的 URL（公开供 Service 使用）
     */
    [[nodiscard]] std::string resolve_url(const std::string &url) const;

private:
    IHttpClient &m_http_client;
    SessionManager m_session_manager;
    Session m_session;
    ConnectionMode m_conn_mode = ConnectionMode::WebVPN;

    /**
     * @brief 将 URL 转换为对应连接模式的 URL
     */
    [[nodiscard]] std::string resolve_url(const std::string &url, ConnectionMode mode) const;

    /**
     * @brief 从 HTML 中提取 CAS execution token
     */
    [[nodiscard]] static std::string extract_execution(const std::string &html);

    /**
     * @brief 构建 CAS 登录表单数据
     */
    [[nodiscard]] static std::string build_login_form(
        const std::string &html, const std::string &username, const std::string &password,
        const std::string &execution, const std::string &captcha);

    /**
     * @brief 从 HTML 中检测错误信息
     */
    [[nodiscard]] static std::string detect_error(const std::string &html);

    /**
     * @brief 手动跟随重定向并提取 Set-Cookie
     */
    Result<HttpResponse> follow_redirects(
        const HttpResponse &response, ConnectionMode mode, int max_redirects = 10,
        const std::string &initial_url = "https://sso.buaa.edu.cn/login");
};

} // namespace UBAANext
