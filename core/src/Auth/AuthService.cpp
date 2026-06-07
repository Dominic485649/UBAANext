/**
 * @file AuthService.cpp
 * @brief 认证服务的实现（支持 mock 和真实 CAS 登录）
 */

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>
#include <UBAANext/Protocol/CasFormParser.hpp>

#include <nlohmann/json.hpp>
#include <cctype>
#include <regex>

namespace UBAANext {

static const char *SSO_LOGIN_URL = "https://sso.buaa.edu.cn/login";
static const char *SSO_WEBVPN_GATEWAY_LOGIN_URL = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fd.buaa.edu.cn%2Flogin%3Fcas_login%3Dtrue";
static const char *WEBVPN_VERIFY_URL = "https://d.buaa.edu.cn/";
static const char *UC_ACTIVATE_URL = "https://uc.buaa.edu.cn/api/login?target=https%3A%2F%2Fuc.buaa.edu.cn%2F%23%2Fuser%2Flogin";
static const char *UC_STATUS_URL = "https://uc.buaa.edu.cn/api/uc/status";
static const char *UC_USERINFO_URL = "https://uc.buaa.edu.cn/api/uc/userinfo";

namespace {

std::string connection_mode_to_string(ConnectionMode mode) {
#if UBAANEXT_ENABLE_MOCKS
    if (mode == ConnectionMode::Mock) return "mock";
#endif
    return mode == ConnectionMode::Direct ? "direct" : "vpn";
}

ConnectionMode connection_mode_from_string(const std::string &mode, ConnectionMode fallback) {
#if UBAANEXT_ENABLE_MOCKS
    if (mode == "mock") return ConnectionMode::Mock;
#endif
    if (mode == "direct") return ConnectionMode::Direct;
    if (mode == "vpn" || mode == "webvpn") return ConnectionMode::WebVPN;
    return fallback;
}

std::string extract_host(const std::string &url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return {};
    }
    auto host_start = scheme_end + 3;
    auto host_end = url.find_first_of("/:?#", host_start);
    std::string host = url.substr(host_start, host_end == std::string::npos ? std::string::npos : host_end - host_start);
    auto at = host.rfind('@');
    if (at != std::string::npos) {
        return {};
    }
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        host = host.substr(0, colon);
    }
    for (auto &ch : host) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return host;
}

bool is_allowed_redirect_url(const std::string &url) {
    if (url.rfind("https://", 0) != 0) {
        return false;
    }
    auto host = extract_host(url);
    return host == "buaa.edu.cn" ||
           (host.size() > 12 && host.compare(host.size() - 12, 12, ".buaa.edu.cn") == 0);
}

void disable_transport_redirects(HttpRequest &request) {
    request.redirect.follow_redirects = false;
}

} // namespace

AuthService::AuthService(IHttpClient &http_client, ISecureStore &secure_store)
    : m_http_client(http_client), m_session_manager(secure_store) {}

#if UBAANEXT_ENABLE_MOCKS
Result<Model::Account> AuthService::login_mock(const std::string &username,
                                                const std::string &password) {
    if (username.empty()) {
        return make_error(ErrorCode::InvalidArgument, "用户名不能为空");
    }
    if (password.empty()) {
        return make_error(ErrorCode::InvalidArgument, "密码不能为空");
    }

    Model::Account account;
    account.student_id = username;
    account.display_name = "Test User";

    auto saved = m_session_manager.save_session(username, account, connection_mode_to_string(m_conn_mode));
    if (!saved) {
        return make_error(saved.error().code, saved.error().message);
    }
    m_session.set_account(account);
    return account;
}
#endif

// ── 真实 CAS 登录 ──────────────────────────────────────────

std::string AuthService::resolve_url(const std::string &url, ConnectionMode mode) const {
    if (mode == ConnectionMode::WebVPN && url.rfind("https://d.buaa.edu.cn/", 0) != 0) {
        return VpnCipher::to_vpn_url(url);
    }
    return url;
}

std::string AuthService::resolve_url(const std::string &url) const {
    return resolve_url(url, m_conn_mode);
}

std::string AuthService::extract_execution(const std::string &html) {
    return Protocol::extract_execution(html);
}

std::string AuthService::build_login_form(
    const std::string &html, const std::string &username, const std::string &password,
    const std::string &execution, const std::string &captcha) {
    auto form = Protocol::build_login_form(html, username, password, execution, captcha);
    if (!captcha.empty() && form.find("captchaResponse=") == std::string::npos) {
        form += "&captchaResponse=" + Protocol::form_url_encode(captcha);
    }
    return form;
}

bool is_ignorable_password_expiry_page(const std::string &html) {
    return (html.find("name=\"execution\"") != std::string::npos || html.find("name='execution'") != std::string::npos) &&
           (html.find("continueForm") != std::string::npos ||
            html.find("ignoreAndContinue") != std::string::npos ||
            html.find("账号存在安全风险") != std::string::npos ||
            html.find("密码过期") != std::string::npos);
}

std::string build_ignore_password_expiry_form(const std::string &execution) {
    return "execution=" + execution + "&_eventId=ignoreAndContinue";
}

std::string AuthService::detect_error(const std::string &html) {
    std::regex re1(R"(<(?:span|div|p)[^>]*(?:class\s*=\s*["'][^"']*(?:tip-text|errors)[^"']*["']|id\s*=\s*["']errorDiv["'])[^>]*>([\s\S]*?)</(?:span|div|p)>)",
                   std::regex::icase);
    std::smatch m;
    if (std::regex_search(html, m, re1) && m.size() > 1) {
        std::string msg = std::regex_replace(m[1].str(), std::regex(R"(<[^>]+>)"), " ");
        auto start = msg.find_first_not_of(" \t\r\n");
        auto end = msg.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            return msg.substr(start, end - start + 1);
        }
    }
    const char *limit_markers[] = {"验证码", "频繁", "限制", "安全风险", "账号存在安全风险", "captcha", "locked"};
    for (const char *marker : limit_markers) {
        if (html.find(marker) != std::string::npos) {
            return "SSO 可能触发验证码、频繁登录限制或风控，请等待一段时间后再尝试";
        }
    }
    return "";
}

Result<HttpResponse> AuthService::follow_redirects(
    const HttpResponse &response, ConnectionMode mode, int max_redirects, const std::string &initial_url) {
    HttpResponse current = response;
    std::string current_url = initial_url;
    for (int i = 0; i < max_redirects; ++i) {
        if (current.status_code < 300 || current.status_code >= 400) {
            if (current.status_code == 200 && is_ignorable_password_expiry_page(current.body)) {
                auto execution = extract_execution(current.body);
                HttpRequest req;
                req.method = HttpMethod::Post;
                req.url = resolve_url(current_url, mode);
                req.headers["Content-Type"] = "application/x-www-form-urlencoded";
                req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
                req.headers["User-Agent"] = "UBAANext/0.4";
                req.headers["Referer"] = resolve_url(current_url, mode);
                req.body = build_ignore_password_expiry_form(execution);
                disable_transport_redirects(req);
                auto result = m_http_client.send(req);
                if (!result) {
                    return result;
                }
                current = *result;
                continue;
            }
            return current;
        }

        std::string location;
        for (const auto &[k, v] : current.headers) {
            if (k == "Location" || k == "location") {
                location = v;
                break;
            }
        }
        if (location.empty()) {
            return current;
        }

        if (location.rfind("http://", 0) != 0 && location.rfind("https://", 0) != 0) {
            auto scheme_end = current_url.find("://");
            auto scheme = (scheme_end == std::string::npos) ? std::string("https") : current_url.substr(0, scheme_end);
            if (location.rfind("//", 0) == 0) {
                location = scheme + ":" + location;
            } else if (!location.empty() && location[0] == '/') {
                auto host_start = (scheme_end == std::string::npos) ? 0 : scheme_end + 3;
                auto path_start = current_url.find('/', host_start);
                auto origin = (path_start == std::string::npos) ? current_url : current_url.substr(0, path_start);
                location = origin + location;
            } else {
                auto slash = current_url.rfind('/');
                location = (slash == std::string::npos) ? location : current_url.substr(0, slash + 1) + location;
            }
        }
        const auto logical_location = VpnCipher::from_vpn_url(location);
        if (!is_allowed_redirect_url(logical_location)) {
            return make_error(ErrorCode::NetworkError, "拒绝不安全的重定向地址");
        }
        current_url = mode == ConnectionMode::Direct ? location : logical_location;

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = mode == ConnectionMode::Direct ? location : resolve_url(logical_location, mode);
        req.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        req.headers["User-Agent"] = "UBAANext/0.4";
        disable_transport_redirects(req);

        auto result = m_http_client.send(req);
        if (!result) {
            return result;
        }
        current = *result;
    }
    return current;
}

Result<void> AuthService::activate_webvpn_session(const std::string &username,
                                                 const std::string &password,
                                                 const std::string &captcha) {
    const auto login_url = resolve_url(SSO_WEBVPN_GATEWAY_LOGIN_URL, ConnectionMode::WebVPN);
    const auto login_logical_url = VpnCipher::from_vpn_url(login_url);

    HttpRequest login_page_req;
    login_page_req.method = HttpMethod::Get;
    login_page_req.url = login_url;
    login_page_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    login_page_req.headers["User-Agent"] = "UBAANext/0.4";
    disable_transport_redirects(login_page_req);

    auto page_result = m_http_client.send(login_page_req);
    if (!page_result) {
        return make_error(ErrorCode::NetworkError, "无法访问 WebVPN 登录页: " + page_result.error().message);
    }

    if (page_result->status_code >= 300 && page_result->status_code < 400) {
        auto final_response = follow_redirects(*page_result, ConnectionMode::Direct, 10, login_logical_url);
        if (!final_response) return make_error(ErrorCode::NetworkError, final_response.error().message);
        if (final_response->status_code == 200 && !extract_execution(final_response->body).empty()) {
            page_result = *final_response;
        } else {
            return Result<void>{};
        }
    }

    auto execution = extract_execution(page_result->body);
    if (execution.empty()) {
        if (page_result->status_code == 200 && page_result->body.find("wengine_vpn") != std::string::npos) {
            return Result<void>{};
        }
        return make_error(ErrorCode::ParseError, "无法从 WebVPN 登录页提取 execution token");
    }

    HttpRequest login_req;
    login_req.method = HttpMethod::Post;
    login_req.url = login_url;
    login_req.headers["Content-Type"] = "application/x-www-form-urlencoded";
    login_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    login_req.headers["User-Agent"] = "UBAANext/0.4";
    login_req.headers["Referer"] = login_url;
    login_req.body = build_login_form(page_result->body, username, password, execution, captcha);
    disable_transport_redirects(login_req);

    auto login_result = m_http_client.send(login_req);
    if (!login_result) {
        return make_error(ErrorCode::NetworkError, "WebVPN CAS 登录请求失败: " + login_result.error().message);
    }

    auto final_response = follow_redirects(*login_result, ConnectionMode::Direct, 10, login_logical_url);
    if (!final_response) return make_error(ErrorCode::NetworkError, final_response.error().message);
    if (final_response->status_code == 200) {
        auto err = detect_error(final_response->body);
        if (!err.empty()) return make_error(ErrorCode::AuthFailed, err);
        if (is_ignorable_password_expiry_page(final_response->body)) {
            auto ignored = follow_redirects(*final_response, ConnectionMode::Direct, 10, login_logical_url);
            if (!ignored) return make_error(ErrorCode::NetworkError, ignored.error().message);
            final_response = *ignored;
        }
        if (!extract_execution(final_response->body).empty()) {
            return make_error(ErrorCode::AuthFailed, "WebVPN 登录失败（用户名或密码错误）");
        }
    }

    HttpRequest verify_req;
    verify_req.method = HttpMethod::Get;
    verify_req.url = WEBVPN_VERIFY_URL;
    verify_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    verify_req.headers["User-Agent"] = "UBAANext/0.4";
    disable_transport_redirects(verify_req);
    auto verify_result = m_http_client.send(verify_req);
    if (!verify_result) return make_error(ErrorCode::NetworkError, "WebVPN 会话验证失败: " + verify_result.error().message);
    if (verify_result->status_code >= 300 && verify_result->status_code < 400) {
        auto verified = follow_redirects(*verify_result, ConnectionMode::Direct, 6, WEBVPN_VERIFY_URL);
        if (!verified) return make_error(ErrorCode::NetworkError, verified.error().message);
        verify_result = *verified;
    }
    if (verify_result->status_code >= 400) {
        return make_error(ErrorCode::AuthFailed, "WebVPN 会话验证返回 " + std::to_string(verify_result->status_code));
    }
    return Result<void>{};
}

Result<Model::Account> AuthService::login_real(
    const std::string &username, const std::string &password,
    ConnectionMode mode, const std::string &captcha) {
    if (username.empty()) {
        return make_error(ErrorCode::InvalidArgument, "用户名不能为空");
    }
    if (password.empty()) {
        return make_error(ErrorCode::InvalidArgument, "密码不能为空");
    }

    // Step 1: GET SSO 登录页
    HttpRequest login_page_req;
    login_page_req.method = HttpMethod::Get;
    login_page_req.url = resolve_url(SSO_LOGIN_URL, mode);
    login_page_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    login_page_req.headers["User-Agent"] = "UBAANext/0.4";
    disable_transport_redirects(login_page_req);

    auto page_result = m_http_client.send(login_page_req);
    if (!page_result) {
        return make_error(ErrorCode::NetworkError, "无法访问 SSO 登录页: " + page_result.error().message);
    }

    // Step 2: 提取 execution token
    std::string execution = extract_execution(page_result->body);
    if (execution.empty()) {
        // 可能已经是登录状态（302 重定向）
        if (page_result->status_code >= 300 && page_result->status_code < 400) {
            // 尝试直接激活 UC
            goto activate_uc;
        }
        return make_error(ErrorCode::ParseError, "无法从 SSO 页面提取 execution token");
    }

    // Step 3: POST 登录表单
    {
        std::string form_body = build_login_form(page_result->body, username, password, execution, captcha);

        HttpRequest login_req;
        login_req.method = HttpMethod::Post;
        login_req.url = resolve_url(SSO_LOGIN_URL, mode);
        login_req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        login_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
        login_req.headers["User-Agent"] = "UBAANext/0.4";
        login_req.headers["Referer"] = resolve_url(SSO_LOGIN_URL, mode);
        login_req.body = form_body;
        disable_transport_redirects(login_req);

        auto login_result = m_http_client.send(login_req);
        if (!login_result) {
            return make_error(ErrorCode::NetworkError, "CAS 登录请求失败: " + login_result.error().message);
        }

        auto final_response = follow_redirects(*login_result, mode, 10, SSO_LOGIN_URL);
        if (!final_response) {
            return make_error(ErrorCode::NetworkError, final_response.error().message);
        }

        if (final_response->status_code == 200) {
            std::string err = detect_error(final_response->body);
            if (!err.empty()) {
                return make_error(ErrorCode::AuthFailed, err);
            }
            if (!extract_execution(final_response->body).empty()) {
                return make_error(ErrorCode::AuthFailed, "登录失败（用户名或密码错误）");
            }
        } else if (final_response->status_code < 300 || final_response->status_code >= 400) {
            std::string err = detect_error(final_response->body);
            if (!err.empty()) {
                return make_error(ErrorCode::AuthFailed, err);
            }
            if (final_response->status_code == 401 || final_response->body.find("Invalid credentials") != std::string::npos) {
                auto body = final_response->body.substr(0, std::min<std::size_t>(final_response->body.size(), 180));
                return make_error(ErrorCode::AuthFailed,
                                  "CAS 登录返回 " + std::to_string(final_response->status_code) + " body=" + body);
            }
        }
    }

activate_uc:
    // Step 5: 激活 UC 登录
    {
        HttpRequest uc_req;
        uc_req.method = HttpMethod::Get;
        uc_req.url = resolve_url(UC_ACTIVATE_URL, mode);
        uc_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
        uc_req.headers["User-Agent"] = "UBAANext/0.4";
        disable_transport_redirects(uc_req);

        auto uc_result = m_http_client.send(uc_req);
        if (!uc_result) {
            return make_error(ErrorCode::NetworkError, "UC 激活失败: " + uc_result.error().message);
        }
        // 跟随重定向
        auto uc_final = follow_redirects(*uc_result, mode, 10, UC_ACTIVATE_URL);
        if (!uc_final) {
            return make_error(ErrorCode::NetworkError, uc_final.error().message);
        }
    }

    // Step 6: 验证会话状态
    {
        HttpRequest status_req;
        status_req.method = HttpMethod::Get;
        status_req.url = resolve_url(UC_STATUS_URL, mode);
        status_req.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
        status_req.headers["X-Requested-With"] = "XMLHttpRequest";
        status_req.headers["User-Agent"] = "UBAANext/0.4";

        auto status_result = m_http_client.send(status_req);
        if (!status_result) {
            return make_error(ErrorCode::NetworkError, "会话验证失败: " + status_result.error().message);
        }

        if (status_result->status_code != 200) {
            return make_error(ErrorCode::AuthFailed, "会话验证返回 " + std::to_string(status_result->status_code));
        }

        // 解析用户信息
        // UC status 返回 JSON: {"data": {"name": "...", "schoolid": "..."}}
        Model::Account account;
        account.student_id = username;

        try {
            auto json = nlohmann::json::parse(status_result->body);
            if (json.value("code", 0) != 0) {
                return make_error(ErrorCode::AuthFailed,
                                  "会话验证失败: code=" + std::to_string(json.value("code", 0)) +
                                      " body=" + status_result->body.substr(0, std::min<std::size_t>(status_result->body.size(), 160)));
            }
            if (!json.contains("data") || !json["data"].is_object()) {
                return make_error(ErrorCode::AuthFailed, "会话验证未返回用户信息");
            }
            auto &data = json["data"];
            auto school_id = data.value("schoolid", data.value("username", ""));
            auto name = data.value("name", data.value("realName", ""));
            if (school_id.empty() && name.empty()) {
                return make_error(ErrorCode::AuthFailed, "会话验证未返回有效用户信息");
            }
            if (!school_id.empty()) {
                account.student_id = school_id;
            }
            if (!name.empty()) {
                account.display_name = name;
            }
        } catch (const std::exception &) {
            return make_error(ErrorCode::ParseError, "无法解析会话验证响应");
        }
        if (account.display_name.empty()) {
            account.display_name = username;
        }

        // Step 7: 获取更详细的用户信息
        {
            HttpRequest info_req;
            info_req.method = HttpMethod::Get;
            info_req.url = resolve_url(UC_USERINFO_URL, mode);
            info_req.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
            info_req.headers["X-Requested-With"] = "XMLHttpRequest";
            info_req.headers["User-Agent"] = "UBAANext/0.4";

            auto info_result = m_http_client.send(info_req);
            if (info_result && info_result->status_code == 200) {
                try {
                    auto json = nlohmann::json::parse(info_result->body);
                    if (json.contains("data") && json["data"].is_object()) {
                        auto &data = json["data"];
                        if (data.contains("name") && data["name"].is_string()) {
                            account.display_name = data["name"].get<std::string>();
                        }
                    }
                } catch (...) {
                    // JSON 解析失败，保留之前的 display_name
                }
            }
        }

        // 持久化并激活会话
        m_conn_mode = mode;
        auto saved = m_session_manager.save_session(username, account, connection_mode_to_string(mode));
        if (!saved) {
            return make_error(saved.error().code, saved.error().message);
        }
        m_session.set_account(account);

        (void)Protocol::Byxt::ensure_session(m_http_client, mode);
        if (mode == ConnectionMode::WebVPN) {
            auto webvpn = activate_webvpn_session(username, password, captcha);
            if (!webvpn) return make_error(webvpn.error().code, webvpn.error().message);
        }

        return account;
    }

}

// ── 原有方法 ──────────────────────────────────────────

Result<void> AuthService::logout() {
    m_session_manager.clear_session();
    m_session.clear();
    return Result<void>{};
}

bool AuthService::has_session() const {
    return m_session.is_valid();
}

const Session &AuthService::session() const {
    return m_session;
}

SessionManager &AuthService::session_manager() {
    return m_session_manager;
}

Result<Model::Account> AuthService::restore_session() {
    auto account = m_session_manager.restore_session();
    if (!account) {
        return make_error(ErrorCode::SessionExpired, "未找到已保存的会话");
    }
    m_conn_mode = connection_mode_from_string(m_session_manager.connection_mode(), m_conn_mode);
    m_session.set_account(*account);
    return std::move(*account);
}

} // namespace UBAANext