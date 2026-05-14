/**
 * @file AuthService.cpp
 * @brief 认证服务的实现（支持 mock 和真实 CAS 登录）
 */

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Net/VpnCipher.hpp>

#include <nlohmann/json.hpp>
#include <cctype>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace UBAANext {

static const char *SSO_LOGIN_URL = "https://sso.buaa.edu.cn/login";
static const char *UC_ACTIVATE_URL = "https://uc.buaa.edu.cn/api/login?target=https%3A%2F%2Fuc.buaa.edu.cn%2F%23%2Fuser%2Flogin";
static const char *UC_STATUS_URL = "https://uc.buaa.edu.cn/api/uc/status";
static const char *UC_USERINFO_URL = "https://uc.buaa.edu.cn/api/uc/userinfo";

namespace {

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

} // namespace

AuthService::AuthService(IHttpClient &http_client, ISecureStore &secure_store)
    : m_http_client(http_client), m_session_manager(secure_store) {}

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

    m_session_manager.save_session(username, account);
    m_session.set_account(account);
    return account;
}

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
    // 匹配 <input name="execution" value="...">
    std::regex re(R"(<input[^>]*name\s*=\s*["']execution["'][^>]*value\s*=\s*["']([^"']*)["'])",
                  std::regex::icase);
    std::smatch m;
    if (std::regex_search(html, m, re) && m.size() > 1) {
        return m[1].str();
    }
    // 尝试反序: value 在 name 之前
    std::regex re2(R"(<input[^>]*value\s*=\s*["']([^"']*)["'][^>]*name\s*=\s*["']execution["'])",
                   std::regex::icase);
    if (std::regex_search(html, m, re2) && m.size() > 1) {
        return m[1].str();
    }
    return "";
}

std::string AuthService::build_login_form(
    const std::string &html, const std::string &username, const std::string &password,
    const std::string &execution, const std::string &captcha) {
    std::string form;
    std::set<std::string> present_names;
    auto add = [&](const std::string &k, const std::string &v) {
        if (!form.empty()) form += "&";
        auto encode = [](const std::string &s) {
            std::ostringstream encoded;
            encoded << std::uppercase << std::hex << std::setfill('0');
            for (unsigned char ch : s) {
                if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '*') {
                    encoded << static_cast<char>(ch);
                } else if (ch == ' ') {
                    encoded << '+';
                } else {
                    encoded << '%' << std::setw(2) << static_cast<int>(ch);
                }
            }
            return encoded.str();
        };
        form += encode(k) + "=" + encode(v);
    };

    std::regex input_re(R"(<input\b([^>]*)>)", std::regex::icase);
    std::regex attr_re(R"(([a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*["']([^"']*)["'])");
    auto inputs_begin = std::sregex_iterator(html.begin(), html.end(), input_re);
    auto inputs_end = std::sregex_iterator();
    for (auto it = inputs_begin; it != inputs_end; ++it) {
        std::string attrs_text = (*it)[1].str();
        std::map<std::string, std::string> attrs;
        auto attrs_begin = std::sregex_iterator(attrs_text.begin(), attrs_text.end(), attr_re);
        for (auto attr_it = attrs_begin; attr_it != inputs_end; ++attr_it) {
            attrs[(*attr_it)[1].str()] = (*attr_it)[2].str();
        }
        auto name_it = attrs.find("name");
        if (name_it == attrs.end() || name_it->second.empty()) {
            continue;
        }
        std::string name = name_it->second;
        auto type_it = attrs.find("type");
        std::string type = type_it != attrs.end() ? type_it->second : "";
        for (auto &ch : type) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        present_names.insert(name);
        if (name == "username" || name == "password" || type == "submit" || type == "button" || type == "image") {
            continue;
        }
        if (type == "checkbox" && attrs_text.find("checked") == std::string::npos) {
            continue;
        }
        auto value_it = attrs.find("value");
        std::string value = value_it != attrs.end() ? value_it->second : "";
        if (type == "hidden" || type == "checkbox" || !value.empty()) {
            add(name, value.empty() && type == "checkbox" ? "on" : value);
        }
    }

    add("username", username);
    add("password", password);
    add("submit", "登录");
    if (present_names.find("execution") == present_names.end()) add("execution", execution);
    if (present_names.find("_eventId") == present_names.end()) add("_eventId", "submit");
    if (present_names.find("type") == present_names.end()) add("type", "username_password");

    if (!captcha.empty()) {
        add("captcha", captcha);
        add("captchaResponse", captcha);
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
        if (!is_allowed_redirect_url(location)) {
            return make_error(ErrorCode::NetworkError, "拒绝不安全的重定向地址");
        }
        current_url = location;

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = resolve_url(location, mode);
        req.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        req.headers["User-Agent"] = "UBAANext/0.4";

        auto result = m_http_client.send(req);
        if (!result) {
            return result;
        }
        current = *result;
    }
    return current;
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
        m_session_manager.save_session(username, account);
        m_session.set_account(account);

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
    m_session.set_account(*account);
    return std::move(*account);
}

} // namespace UBAANext