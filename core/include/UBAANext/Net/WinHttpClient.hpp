/**
 * @file WinHttpClient.hpp
 * @brief 基于 WinHTTP 的真实 HTTP 客户端
 *
 * 实现 IHttpClient 接口，使用 Windows WinHTTP API。
 * 支持 HTTPS、Cookie 持久化、重定向控制、超时配置。
 */
#pragma once

#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Net/HttpClient.hpp>

#include <string>

#if !defined(_WIN32)
#error "WinHttpClient is only available on Windows."
#endif

#include <windows.h>
#include <winhttp.h>

namespace UBAANext {

/**
 * @brief WinHTTP 客户端配置
 */
struct WinHttpConfig {
    int connect_timeout_ms = 10000;   ///< 连接超时
    int request_timeout_ms = 30000;   ///< 请求超时
    bool follow_redirects = true;     ///< 是否自动跟随重定向
    std::string proxy;                ///< 代理地址（空=不使用代理）
    std::string user_agent = "UBAANext/0.4";
};

/**
 * @brief 基于 WinHTTP 的 HTTP 客户端实现
 *
 * 线程安全：非线程安全。
 * 每个实例维护独立的 WinHTTP session 和 cookie jar。
 */
class WinHttpClient : public IHttpClient {
public:
    explicit WinHttpClient(const WinHttpConfig &config = {});
    ~WinHttpClient() override;

    WinHttpClient(const WinHttpClient &) = delete;
    WinHttpClient &operator=(const WinHttpClient &) = delete;

    [[nodiscard]] Result<HttpResponse> send(const HttpRequest &request) override;

    /**
     * @brief 获取 CookieJar（用于手动管理 cookie）
     */
    [[nodiscard]] CookieJar &cookies() { return m_cookies; }
    [[nodiscard]] const CookieJar &cookies() const { return m_cookies; }

    /**
     * @brief 保存 cookie 到文件
     */
    void save_cookies(const std::string &path);

    /**
     * @brief 从文件加载 cookie
     */
    void load_cookies(const std::string &path);

    /**
     * @brief 设置是否跟随重定向
     */
    void set_follow_redirects(bool follow) { m_config.follow_redirects = follow; }

    /**
     * @brief RAII 临时切换重定向设置
     *
     * 构造时保存原值并设置新值，析构时恢复。
     */
    class RedirectGuard {
    public:
        RedirectGuard(WinHttpClient &client, bool follow)
            : m_client(client), m_prev(client.m_config.follow_redirects) {
            client.m_config.follow_redirects = follow;
        }
        ~RedirectGuard() { m_client.m_config.follow_redirects = m_prev; }
    private:
        WinHttpClient &m_client;
        bool m_prev;
    };

    [[nodiscard]] RedirectGuard scoped_redirects(bool follow) { return {*this, follow}; }

private:
    WinHttpConfig m_config;
    CookieJar m_cookies;
    HINTERNET m_session = nullptr;

    bool init_session();
    static std::wstring to_wstring(const std::string &s);
    static std::string from_wstring(const std::wstring &ws);
};

} // namespace UBAANext
