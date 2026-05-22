#include <UBAANext/Protocol/AppBuaaSession.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/CasFormParser.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

namespace UBAANext::Protocol::AppBuaa {

namespace {

std::string redirect_location(const HttpResponse &response) {
    return Protocol::header_value(response, "Location");
}

std::string resolve_app_location(const std::string &location, const std::string &current_url) {
    return Protocol::resolve_location(current_url, location);
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
    return Protocol::is_session_expired_response(response);
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
    if (response->status_code == 200 && !username.empty() && !password.empty() && !Protocol::extract_execution(response->body).empty()) {
        auto execution = Protocol::extract_execution(response->body);
        HttpRequest submit;
        submit.method = HttpMethod::Post;
        submit.url = resolve_url(redirect_url, mode);
        submit.headers["Content-Type"] = "application/x-www-form-urlencoded";
        submit.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
        submit.headers["User-Agent"] = user_agent;
        submit.headers["Referer"] = resolve_url(redirect_url, mode);
        submit.body = Protocol::build_login_form(response->body, username, password, execution);
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
        current_url = resolve_app_location(location, current_url);

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
        return make_error(ErrorCode::SessionExpired,
                          "app.buaa 会话同步失败: status=" + std::to_string(response->status_code));
    }
    return {};
}

} // namespace UBAANext::Protocol::AppBuaa
