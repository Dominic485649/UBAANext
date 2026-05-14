#include <UBAANext/Protocol/ByxtSession.hpp>

#include <UBAANext/Net/VpnCipher.hpp>

#include <algorithm>

namespace UBAANext::Protocol::Byxt {

namespace {

constexpr const char *kCurrentUserUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/currentUser.do";
constexpr const char *kSsoServiceUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbyxt.buaa.edu.cn%2Fjwapp%2Fsys%2Fhomeapp%2Fapi%2Fhome%2FcurrentUser.do";
constexpr const char *kUserAgent = "UBAANext/0.4";

std::string redirect_location(const HttpResponse &response) {
    for (const auto &[key, value] : response.headers) {
        if (key == "Location" || key == "location") {
            return value;
        }
    }
    return {};
}

bool is_sso_login_page(const std::string &body) {
    return body.find("input name=\"execution\"") != std::string::npos ||
           body.find("统一身份认证") != std::string::npos;
}

std::string trim_start_copy(const std::string &body) {
    auto first = body.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : body.substr(first);
}

bool is_json_body(const std::string &body) {
    auto trimmed = trim_start_copy(body);
    return !trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[');
}

bool session_expired_body(const std::string &body) {
    return is_sso_login_page(body) ||
           body.find("您的会话已经过期") != std::string::npos ||
           body.find("会话已经过期") != std::string::npos ||
           body.find("\"url\":\"/login\"") != std::string::npos ||
           body.find("\"url\": \"/login\"") != std::string::npos;
}

std::string extract_origin(const std::string &url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return "https://byxt.buaa.edu.cn";
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
    if (!location.empty() && location.front() == '/') {
        return extract_origin(current_url) + location;
    }
    auto last_slash = current_url.rfind('/');
    if (last_slash != std::string::npos) {
        return current_url.substr(0, last_slash + 1) + location;
    }
    return current_url + "/" + location;
}

Result<void> follow_redirect_chain(IHttpClient &http_client,
                                   ConnectionMode mode,
                                   HttpResponse response,
                                   const std::string &initial_url,
                                   int max_redirects = 15) {
    std::string current_url = initial_url;

    for (int i = 0; i < max_redirects; ++i) {
        if (response.status_code < 300 || response.status_code >= 400) {
            if (is_sso_login_page(response.body)) {
                return make_error(ErrorCode::SessionExpired, "到达 SSO 登录页，会话需要重新认证");
            }
            return {};
        }

        auto location = redirect_location(response);
        if (location.empty()) {
            return make_error(ErrorCode::NetworkError, "重定向缺少 Location");
        }

        std::string target_url = resolve_location(location, current_url);
        current_url = target_url;

        HttpRequest next;
        next.method = HttpMethod::Get;
        next.url = resolve_url(target_url, mode);
        next.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        next.headers["User-Agent"] = kUserAgent;

        auto next_result = http_client.send(next);
        if (!next_result) {
            return make_error(ErrorCode::NetworkError, "重定向请求失败: " + next_result.error().message);
        }
        response = *next_result;
    }

    return make_error(ErrorCode::NetworkError, "重定向次数过多");
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

void apply_ajax_headers(HttpRequest &request, ConnectionMode mode, const std::string &referer) {
    request.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["Referer"] = resolve_url(referer, mode);
    request.headers["User-Agent"] = kUserAgent;
}

bool is_session_expired_response(const HttpResponse &response) {
    return response.status_code == 401 || response.status_code == 403 ||
           session_expired_body(response.body) || !is_json_body(response.body);
}

Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode) {
    HttpRequest probe;
    probe.method = HttpMethod::Get;
    probe.url = resolve_url(kCurrentUserUrl, mode);
    apply_ajax_headers(probe, mode);

    auto probe_result = http_client.send(probe);
    if (probe_result && probe_result->status_code == 200 && !is_session_expired_response(*probe_result)) {
        return {};
    }

    HttpRequest app_req;
    app_req.method = HttpMethod::Get;
    app_req.url = resolve_url(kSsoServiceUrl, mode);
    app_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    app_req.headers["User-Agent"] = kUserAgent;

    auto app_result = http_client.send(app_req);
    if (!app_result) {
        return make_error(ErrorCode::NetworkError, "BYXT 应用页请求失败: " + app_result.error().message);
    }

    if (app_result->status_code >= 300 && app_result->status_code < 400) {
        auto redirect_result = follow_redirect_chain(http_client, mode, *app_result, kSsoServiceUrl);
        if (!redirect_result) {
            return redirect_result;
        }
    } else if (app_result->status_code == 401 || app_result->status_code == 403 || is_sso_login_page(app_result->body)) {
        auto trimmed = trim_start_copy(app_result->body);
        std::string prefix = trimmed.substr(0, std::min<std::size_t>(trimmed.size(), 80));
        return make_error(ErrorCode::SessionExpired,
                          "需要 SSO 认证: appStatus=" + std::to_string(app_result->status_code) +
                              " body=" + prefix);
    }

    probe_result = http_client.send(probe);
    if (probe_result && probe_result->status_code == 200 && !is_session_expired_response(*probe_result)) {
        return {};
    }

    if (probe_result) {
        auto trimmed = trim_start_copy(probe_result->body);
        std::string prefix = trimmed.substr(0, std::min<std::size_t>(trimmed.size(), 80));
        auto app_trimmed = trim_start_copy(app_result->body);
        std::string app_prefix = app_trimmed.substr(0, std::min<std::size_t>(app_trimmed.size(), 80));
        return make_error(ErrorCode::SessionExpired,
                          "BYXT 会话激活失败: appStatus=" + std::to_string(app_result->status_code) +
                              " appBody=" + app_prefix + " probeStatus=" + std::to_string(probe_result->status_code) +
                              " probeBody=" + prefix);
    }
    return make_error(ErrorCode::SessionExpired, "BYXT 会话激活失败: 无探测响应");
}

} // namespace UBAANext::Protocol::Byxt
