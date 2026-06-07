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
 *
 * MockOnly 仅用于离线合同验证；Direct/WebVPN 会触发真实远端认证与下游请求。
 */
enum class ConnectionMode {
#if UBAANEXT_ENABLE_MOCKS
    Mock,       ///< MockOnly：模拟数据，不证明真实 UBAA 语义
#endif
    Direct,     ///< 真实内网直连，可能触发远端请求
    WebVPN,     ///< 真实 WebVPN 网关路径，可能触发远端请求
};

/**
 * @brief 高级认证服务
 *
 * Sensitive input/session boundary：编排登录、登出、会话恢复和 URL 解析。
 * 真实登录会触发远端 CAS 请求并保存账户/session 数据；MockOnly 登录只验证离线合同。
 */
class AuthService {
public:
    AuthService(IHttpClient &http_client, ISecureStore &secure_store);

#if UBAANEXT_ENABLE_MOCKS
    /**
     * @brief MockOnly 登录入口，不发起网络请求，不证明真实 CAS/session 语义。
     *
     * Sensitive input：username/password 仍不得写入日志或 diagnostics。
     */
    Result<Model::Account> login_mock(const std::string &username,
                                      const std::string &password);
#endif

    /**
     * @brief Sensitive input：执行真实 CAS 登录，会触发远端请求并保存 session/account 数据。
     * @param username 学号，不得输出到错误、日志或 diagnostics
     * @param password 密码，不得输出到错误、日志或 diagnostics
     * @param mode     连接模式（Direct 或 WebVPN）
     * @param captcha  验证码（可选，如果 CAS 要求），不得输出到错误、日志或 diagnostics
     * @return 登录成功后的账户信息；认证、网络、解析或存储失败必须稳定返回错误
     */
    Result<Model::Account> login_real(const std::string &username,
                                      const std::string &password,
                                      ConnectionMode mode,
                                      const std::string &captcha = "");

    /** Sensitive session boundary: clears stored session/account data without proving remote logout. */
    Result<void> logout();
    /** Sensitive session boundary: true only means local session data is present. */
    [[nodiscard]] bool has_session() const;
    /** Sensitive output: returned account/session fields must remain redaction-aware. */
    [[nodiscard]] const Session &session() const;
    /** Sensitive session boundary: exposes persistence manager; callers must not bypass secure-store semantics. */
    [[nodiscard]] SessionManager &session_manager();
    /** Sensitive output: restores persisted session/account data or returns stable storage/session errors. */
    Result<Model::Account> restore_session();

    /** Sensitive connection boundary: selects how later URLs resolve; does not prove platform capability. */
    void set_connection_mode(ConnectionMode mode) { m_conn_mode = mode; }
    /** Sensitive connection boundary: exposes current routing mode for downstream requests. */
    [[nodiscard]] ConnectionMode connection_mode() const { return m_conn_mode; }

    /**
     * @brief PartiallyMigrated URL resolver for Direct/WebVPN service calls; must not log sensitive query strings.
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
     * @brief Sensitive parser helper: extracts CAS execution token from HTML and must not log raw login pages.
     */
    [[nodiscard]] static std::string extract_execution(const std::string &html);

    /**
     * @brief Sensitive input helper: builds CAS login form containing username/password/captcha.
     */
    [[nodiscard]] static std::string build_login_form(
        const std::string &html, const std::string &username, const std::string &password,
        const std::string &execution, const std::string &captcha);

    /**
     * @brief Sensitive parser helper: detects CAS errors without exposing raw HTML.
     */
    [[nodiscard]] static std::string detect_error(const std::string &html);

    /**
     * @brief Sensitive WebVPN boundary: activates the WebVPN gateway CAS session without logging credentials.
     */
    Result<void> activate_webvpn_session(const std::string &username,
                                         const std::string &password,
                                         const std::string &captcha);

    /**
     * @brief PartiallyMigrated redirect helper: follows CAS redirects and extracts cookies with redacted diagnostics.
     */
    Result<HttpResponse> follow_redirects(
        const HttpResponse &response, ConnectionMode mode, int max_redirects = 10,
        const std::string &initial_url = "https://sso.buaa.edu.cn/login");
};

} // namespace UBAANext
