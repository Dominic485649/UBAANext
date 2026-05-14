#include <UBAANext/Protocol/AppBuaaSession.hpp>

#include <UBAANext/Net/VpnCipher.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace UBAANext::Protocol::AppBuaa {

namespace {

std::string redirect_location(const HttpResponse &response) {
    auto location = response.headers.find("Location");
    if (location != response.headers.end()) {
        return location->second;
    }
    auto lower_location = response.headers.find("location");
    return lower_location != response.headers.end() ? lower_location->second : std::string{};
}

std::string extract_origin(const std::string &url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return "https://app.buaa.edu.cn";
    }
    auto host_start = scheme_end + 3;
    auto path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        return url;
    }
    return url.substr(0, path_start);
}

std::string resolve_location(const std::string &location, const std::string &current_url) {
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
        return location;
    }
    if (location.rfind("//", 0) == 0) {
        auto scheme_end = current_url.find("://");
        auto scheme = scheme_end == std::string::npos ? std::string{"https"} : current_url.substr(0, scheme_end);
        return scheme + ":" + location;
    }
    if (!location.empty() && location.front() == '/') {
        return extract_origin(current_url) + location;
    }
    auto last_slash = current_url.rfind('/');
    if (last_slash != std::string::npos) {
        return current_url.substr(0, last_slash + 1) + location;
    }
    return current_url + "/" + location;
}

std::string encode_form(const std::string &s) {
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
}

std::string extract_execution(const std::string &html) {
    std::regex re(R"(<input[^>]*name\s*=\s*["']execution["'][^>]*value\s*=\s*["']([^"']*)["'])", std::regex::icase);
    std::smatch m;
    if (std::regex_search(html, m, re) && m.size() > 1) {
        return m[1].str();
    }
    std::regex re2(R"(<input[^>]*value\s*=\s*["']([^"']*)["'][^>]*name\s*=\s*["']execution["'])", std::regex::icase);
    if (std::regex_search(html, m, re2) && m.size() > 1) {
        return m[1].str();
    }
    return {};
}

std::string build_login_form(const std::string &html,
                             const std::string &username,
                             const std::string &password,
                             const std::string &execution) {
    std::string form;
    std::set<std::string> present_names;
    auto add = [&](const std::string &key, const std::string &value) {
        if (!form.empty()) form += "&";
        form += encode_form(key) + "=" + encode_form(value);
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
    return form;
}

} // namespace

std::string resolve_url(const std::string &url, ConnectionMode mode) {
    if (mode != ConnectionMode::WebVPN) {
        return url;
    }
    if (url.rfind("https://d.buaa.edu.cn/", 0) == 0) {
        return url;
    }
    return VpnCipher::to_vpn_url(url);
}

void apply_ajax_headers(HttpRequest &request, ConnectionMode mode, const std::string &referer, const std::string &user_agent) {
    request.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["Referer"] = resolve_url(referer, mode);
    request.headers["User-Agent"] = user_agent;
}

bool is_session_expired_response(const HttpResponse &response) {
    auto location = response.headers.find("Location");
    auto lower_location = response.headers.find("location");
    std::string redirect = location != response.headers.end() ? location->second :
        lower_location != response.headers.end() ? lower_location->second : std::string{};
    return response.status_code == 401 || response.status_code == 403 ||
           redirect.find("sso.buaa.edu.cn") != std::string::npos ||
           response.body.find("统一身份认证") != std::string::npos ||
           response.body.find("input name=\"execution\"") != std::string::npos;
}

Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent) {
    return ensure_session(http_client, mode, redirect_url, user_agent, {}, {});
}

Result<void> ensure_session(IHttpClient &http_client,
                            ConnectionMode mode,
                            const std::string &redirect_url,
                            const std::string &user_agent,
                            const std::string &username,
                            const std::string &password) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_url(redirect_url, mode);
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
    request.headers["User-Agent"] = user_agent;

    auto response = http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "app.buaa 会话同步失败: " + response.error().message);
    }
    if (response->status_code == 200 && !username.empty() && !password.empty() && !extract_execution(response->body).empty()) {
        HttpRequest submit;
        submit.method = HttpMethod::Post;
        submit.url = resolve_url(redirect_url, mode);
        submit.headers["Content-Type"] = "application/x-www-form-urlencoded";
        submit.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
        submit.headers["User-Agent"] = user_agent;
        submit.headers["Referer"] = resolve_url(redirect_url, mode);
        submit.body = build_login_form(response->body, username, password, extract_execution(response->body));
        response = http_client.send(submit);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "app.buaa 登录提交失败: " + response.error().message);
        }
    }

    std::string current_url = redirect_url;
    for (int i = 0; i < 12 && response->status_code >= 300 && response->status_code < 400; ++i) {
        auto location = redirect_location(*response);
        if (location.empty()) {
            return make_error(ErrorCode::NetworkError, "app.buaa 会话同步重定向缺少 Location");
        }
        current_url = resolve_location(location, current_url);

        HttpRequest next;
        next.method = HttpMethod::Get;
        next.url = resolve_url(current_url, mode);
        next.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        next.headers["User-Agent"] = user_agent;
        response = http_client.send(next);
        if (!response) {
            return make_error(ErrorCode::NetworkError, "app.buaa 会话同步重定向失败: " + response.error().message);
        }
    }

    if (response->status_code < 200 || response->status_code >= 400 || is_session_expired_response(*response)) {
        auto prefix = response->body.substr(0, std::min<std::size_t>(response->body.size(), 120));
        return make_error(ErrorCode::SessionExpired,
                          "app.buaa 会话同步失败: status=" + std::to_string(response->status_code) + " body=" + prefix);
    }
    return {};
}

} // namespace UBAANext::Protocol::AppBuaa
