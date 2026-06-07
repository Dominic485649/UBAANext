/**
 * @file CookieJar.hpp
 * @brief HTTP 会话的内存 Cookie 存储
 *
 * 本文件定义了 CookieJar 类，用于在 HTTP 会话期间管理 Cookie。
 * CookieJar 提供简单的键值 Cookie 存储，并支持将所有 Cookie
 * 序列化为标准的 "Cookie:" HTTP 请求头格式。
 *
 * 设计背景：
 *   在 UBAANext 与后端 API 通信时，某些接口（如登录、设备绑定）
 *   需要维持会话状态。服务器通过 Set-Cookie 响应头下发会话标识
 *   （如 JSESSIONID），客户端需要在后续请求中通过 Cookie 请求头
 *   回传该标识以保持会话。
 *
 * 使用场景：
 *   1. 用户登录成功后，服务器返回 Set-Cookie: JSESSIONID=abc123
 *   2. CookieJar 存储该 Cookie
 *   3. 后续请求自动在请求头中附加 Cookie: JSESSIONID=abc123
 *   4. 服务器识别会话并返回正确的用户数据
 *
 * @attention Sensitive cookie storage: this jar stores session identifiers in memory and implements
 *   the host/domain and path matching required by the current campus services. It intentionally remains
 *   a small RFC 6265 subset: expiration and Secure/HttpOnly/SameSite attributes are handled outside this
 *   value object or ignored where not needed. Serialized/header output must never be logged verbatim.
 *
 * @author UBAANext Team
 * @version 0.2
 */
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace UBAANext {

/**
 * @brief 用于 HTTP 会话管理的内存 Cookie 罐
 *
 * CookieJar 以简单的名称-值对形式存储 Cookie，提供存取、删除、
 * 清空和序列化等操作。Cookie 数据仅保存在内存中，程序退出后
 * 不会持久化。
 *
 * 典型使用流程：
 * @code
 *   CookieJar jar;
 *
 *   // 从服务器响应的 Set-Cookie 头中提取 Cookie 并存储
 *   jar.set_cookie("JSESSIONID", "abc123");
 *   jar.set_cookie("theme", "dark");
 *
 *   // 构建 HTTP 请求时，将 Cookie 添加到请求头
 *   HttpRequest req;
 *   req.url = "https://api.example.com/data";
 *   req.headers["Cookie"] = jar.to_header();
 *   // req.headers["Cookie"] 的值为 "JSESSIONID=abc123; theme=dark"
 *
 *   // 检查特定 Cookie 是否存在
 *   if (auto sid = jar.get_cookie("JSESSIONID"); sid.has_value()) {
 *       // 使用 sid.value() ...
 *   }
 *
 *   // 退出登录时清空所有 Cookie
 *   jar.clear();
 * @endcode
 *
 * Sensitive output：Cookie header、serialized lines 和 cookie values 可能包含 session/token，必须脱敏。
 *
 * 线程安全性：
 *   CookieJar 不是线程安全的。如果需要在多线程环境中使用，
 *   调用方应使用 std::mutex 等同步机制保护并发访问。
 */
class CookieJar {
public:
    /**
     * @brief Sensitive input：存储或覆盖一个 Cookie，可能包含 session/token。
     *
     * 将指定名称和值的 Cookie 存入内部映射表。如果同名 Cookie
     * 已存在，则用新值覆盖旧值。
     *
     * 参数使用 std::string（而非 string_view），因为需要将值
     * 拷贝到内部存储中。调用方可使用 std::move 语义避免拷贝。
     *
     * @param name  Cookie 名称（如 "JSESSIONID"）。支持移动语义。
     * @param value Cookie 值（如 "abc123"）。支持移动语义。
     *
     * @note name 和 value 不能为空字符串。虽然当前实现不强制检查，
     *       但空名称的 Cookie 在 HTTP 规范中没有意义。
     */
    void set_cookie(std::string name, std::string value);
    /** Sensitive input: host-scoped cookie storage. */
    void set_cookie(std::string host, std::string name, std::string value);
    /** Sensitive input: host/path-scoped cookie storage. */
    void set_cookie(std::string host, std::string path, std::string name, std::string value);

    /**
     * @brief Sensitive output：根据名称检索 Cookie 值，调用方不得记录原值。
     */
    [[nodiscard]] std::optional<std::string> get_cookie(std::string_view name) const;
    /** Sensitive output: host lookup returns a value that must remain redacted. */
    [[nodiscard]] std::optional<std::string> get_cookie(std::string_view host, std::string_view name) const;

    /**
     * @brief Sensitive session boundary：根据名称移除一个 Cookie，不证明远端 session 已失效。
     */
    void remove_cookie(std::string_view name);
    void remove_cookie(std::string_view host, std::string_view name);

    /**
     * @brief Sensitive session boundary：移除所有已存储的 Cookie，不证明远端 logout 已执行。
     */
    void clear();

    /**
     * @brief Sensitive output：将 Cookie 序列化为 HTTP "Cookie:" 请求头值，必须脱敏后才能输出。
     */
    [[nodiscard]] std::string to_header() const;
    /** Sensitive output: host-scoped Cookie header must not be logged verbatim. */
    [[nodiscard]] std::string to_header(std::string_view host) const;
    /** Sensitive output: host/path-scoped Cookie header must not be logged verbatim. */
    [[nodiscard]] std::string to_header(std::string_view host, std::string_view path) const;

    /** Sensitive output: serialized cookie lines may contain session identifiers. */
    [[nodiscard]] std::vector<std::string> serialize() const;
    /** Sensitive input: loads serialized cookie lines from platform persistence. */
    void load_serialized_line(const std::string &line);

private:
    struct CookieKey {
        std::string host;
        std::string path;
        std::string name;

        [[nodiscard]] bool operator<(const CookieKey &other) const noexcept {
            if (host != other.host) {
                return host < other.host;
            }
            if (path != other.path) {
                return path < other.path;
            }
            return name < other.name;
        }
    };

    std::map<CookieKey, std::string> m_cookies;
};

} // namespace UBAANext
