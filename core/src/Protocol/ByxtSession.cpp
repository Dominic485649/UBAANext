#include <UBAANext/Protocol/ByxtSession.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

#include <algorithm>
#include <cctype>

namespace UBAANext::Protocol::Byxt {

namespace {

constexpr const char *kCurrentUserUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/currentUser.do";
constexpr const char *kByxtIndexUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/index.html";
constexpr const char *kGraduateUserInfoUrl = "https://gsmis.buaa.edu.cn/gsapp/sys/yjsemaphome/modules/pubWork/getUserInfo.do";
constexpr const char *kSsoServiceUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbyxt.buaa.edu.cn%2Fjwapp%2Fsys%2Fhomeapp%2Fapi%2Fhome%2FcurrentUser.do";
constexpr const char *kUserAgent = "UBAANext/0.4";

enum class PortalProbeResult {
    UndergradReady,
    GraduateReady,
    SsoRequired,
    Unavailable,
};

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

Result<void> follow_redirect_chain(IHttpClient &http_client,
                                   ConnectionMode mode,
                                   HttpResponse response,
                                   const std::string &initial_url,
                                   int max_redirects = 15);

Result<void> request_html_page(IHttpClient &http_client, ConnectionMode mode, const std::string &url);

std::string redirect_location(const HttpResponse &response) {
    return Protocol::header_value(response, "Location");
}

bool is_sso_login_page(const std::string &body) {
    return Protocol::has_sso_login_marker(body);
}

std::string trim_start_copy(const std::string &body) {
    auto first = body.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? std::string{} : body.substr(first);
}

bool is_json_body(const std::string &body) {
    auto trimmed = trim_start_copy(body);
    return !trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[');
}

bool is_html_body(const std::string &body) {
    auto trimmed = to_lower_copy(trim_start_copy(body));
    return trimmed.rfind("<!doctype html", 0) == 0 || trimmed.rfind("<html", 0) == 0;
}

bool is_sso_response(const HttpResponse &response, const std::string &final_url = {}) {
    if (response.status_code == 401 || response.status_code == 403) {
        return true;
    }
    const auto location = redirect_location(response);
    return Protocol::is_sso_or_login_url(location) ||
           (!final_url.empty() && Protocol::is_sso_or_login_url(final_url)) ||
           Protocol::has_session_expired_marker(response.body) ||
           (is_html_body(response.body) && Protocol::has_sso_login_marker(response.body));
}

PortalProbeResult classify_undergrad_response(const HttpResponse &response, const std::string &final_url) {
    if (is_sso_response(response, final_url)) {
        return PortalProbeResult::SsoRequired;
    }
    if (response.status_code != 200) {
        return PortalProbeResult::Unavailable;
    }

    auto lowered_final_url = to_lower_copy(final_url);
    if (lowered_final_url.find("/jwapp/sys/byrhmhsy/") != std::string::npos) {
        return is_json_body(response.body) || trim_start_copy(response.body).empty()
            ? PortalProbeResult::GraduateReady
            : PortalProbeResult::Unavailable;
    }

    if (is_html_body(response.body)) {
        return PortalProbeResult::Unavailable;
    }
    return is_json_body(response.body) ? PortalProbeResult::UndergradReady : PortalProbeResult::Unavailable;
}

PortalProbeResult classify_graduate_response(const HttpResponse &response, const std::string &final_url) {
    if (is_sso_response(response, final_url)) {
        return PortalProbeResult::SsoRequired;
    }
    if (response.status_code != 200) {
        return PortalProbeResult::Unavailable;
    }
    return (response.body.find(R"("code":"0")") != std::string::npos ||
            response.body.find(R"("code": "0")") != std::string::npos ||
            response.body.find(R"('code':'0')") != std::string::npos ||
            response.body.find(R"('code': '0')") != std::string::npos) &&
            (response.body.find(R"("userId")") != std::string::npos ||
             response.body.find(R"("userName")") != std::string::npos)
        ? PortalProbeResult::GraduateReady
        : PortalProbeResult::Unavailable;
}

Result<PortalProbeResult> probe_portal(IHttpClient &http_client,
                                       ConnectionMode mode,
                                       const std::string &url,
                                       PortalProbeResult (*classifier)(const HttpResponse &, const std::string &)) {
    HttpRequest probe;
    probe.method = HttpMethod::Get;
    probe.url = resolve_url(url, mode);
    apply_ajax_headers(probe, mode);

    auto response = http_client.send(probe);
    if (!response) {
        return make_error(response.error().code, response.error().message);
    }
    return classifier(*response, url);
}

Result<PortalProbeResult> probe_academic_portal(IHttpClient &http_client, ConnectionMode mode) {
    auto undergrad = probe_portal(http_client, mode, kCurrentUserUrl, classify_undergrad_response);
    if (undergrad && *undergrad == PortalProbeResult::UndergradReady) {
        return *undergrad;
    }

    auto graduate = probe_portal(http_client, mode, kGraduateUserInfoUrl, classify_graduate_response);
    if (graduate && *graduate == PortalProbeResult::GraduateReady) {
        return *graduate;
    }

    if ((undergrad && *undergrad == PortalProbeResult::SsoRequired) ||
        (graduate && *graduate == PortalProbeResult::SsoRequired)) {
        auto warmed = request_html_page(http_client, mode, kByxtIndexUrl);
        if (warmed) {
            undergrad = probe_portal(http_client, mode, kCurrentUserUrl, classify_undergrad_response);
            if (undergrad && *undergrad == PortalProbeResult::UndergradReady) {
                return *undergrad;
            }
        }
        return PortalProbeResult::SsoRequired;
    }
    if (!undergrad && !graduate) {
        return make_error(undergrad.error().code, undergrad.error().message);
    }
    return PortalProbeResult::Unavailable;
}

Result<void> request_html_page(IHttpClient &http_client, ConnectionMode mode, const std::string &url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_url(url, mode);
    request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
    request.headers["User-Agent"] = kUserAgent;
    Protocol::disable_transport_redirects(request);

    auto response = http_client.send(request);
    if (!response) {
        return make_error(response.error().code, response.error().message);
    }
    if (response->status_code >= 300 && response->status_code < 400) {
        return follow_redirect_chain(http_client, mode, *response, url);
    }
    if (is_sso_response(*response)) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::RedirectFollow,
                                           DownstreamSessionState::SsoRequired,
                                           "BYXT warm-up 需要 SSO 认证: status=" + std::to_string(response->status_code),
                                           redact_url_query(url));
        return make_error(error.code, to_error(error).message);
    }
    if (response->status_code < 200 || response->status_code >= 400) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::RedirectFollow,
                                           DownstreamSessionState::Unavailable,
                                           "BYXT warm-up 页面不可用: status=" + std::to_string(response->status_code),
                                           redact_url_query(url));
        return make_error(error.code, to_error(error).message);
    }
    return {};
}

Result<void> follow_redirect_chain(IHttpClient &http_client,
                                   ConnectionMode mode,
                                   HttpResponse response,
                                   const std::string &initial_url,
                                   int max_redirects) {
    auto observe = [](const RedirectStep &step) -> Result<void> {
        if (is_sso_login_page(step.response.body)) {
            auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                               DownstreamActivationStage::RedirectFollow,
                                               DownstreamSessionState::SsoRequired,
                                               "到达 SSO 登录页，会话需要重新认证",
                                               redact_url_query(step.request_url));
            return make_error(error.code, to_error(error).message);
        }
        return {};
    };

    if (response.status_code < 300 || response.status_code >= 400) {
        return observe(RedirectStep{initial_url, resolve_url(initial_url, mode), std::move(response)});
    }

    auto location = redirect_location(response);
    if (location.empty()) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::RedirectFollow,
                                           DownstreamSessionState::ProtocolError,
                                           "重定向缺少 Location",
                                           redact_url_query(initial_url));
        return make_error(error.code, to_error(error).message);
    }

    auto start_url = Protocol::resolve_location(initial_url, location);
    auto navigated = Protocol::follow_redirects(http_client,
                                                mode,
                                                start_url,
                                                DownstreamSystemId::Byxt,
                                                [](const std::string &url, ConnectionMode mode) { return resolve_url(url, mode); },
                                                [](HttpRequest &request) {
                                                    request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
                                                    request.headers["User-Agent"] = kUserAgent;
                                                },
                                                observe,
                                                max_redirects);
    if (!navigated) {
        return make_error(navigated.error().code, navigated.error().message);
    }
    return {};
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
    Protocol::disable_transport_redirects(request);
}

bool is_session_expired_response(const HttpResponse &response) {
    return classify_undergrad_response(response, kCurrentUserUrl) != PortalProbeResult::UndergradReady;
}

Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode) {
    auto probe_result = probe_academic_portal(http_client, mode);
    if (probe_result && (*probe_result == PortalProbeResult::UndergradReady || *probe_result == PortalProbeResult::GraduateReady)) {
        return {};
    }

    HttpRequest app_req;
    app_req.method = HttpMethod::Get;
    app_req.url = resolve_url(kSsoServiceUrl, mode);
    app_req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    app_req.headers["User-Agent"] = kUserAgent;
    Protocol::disable_transport_redirects(app_req);

    auto app_result = http_client.send(app_req);
    if (!app_result) {
        return make_error(ErrorCode::NetworkError, "BYXT 应用页请求失败: " + app_result.error().message);
    }

    if (app_result->status_code >= 300 && app_result->status_code < 400) {
        auto redirect_result = follow_redirect_chain(http_client, mode, *app_result, kSsoServiceUrl);
        if (!redirect_result) {
            return redirect_result;
        }
    } else if (is_sso_response(*app_result)) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::Probe,
                                           DownstreamSessionState::SsoRequired,
                                           "需要 SSO 认证: appStatus=" + std::to_string(app_result->status_code));
        return make_error(error.code, to_error(error).message);
    } else if (app_result->status_code != 200) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::Probe,
                                           DownstreamSessionState::Unavailable,
                                           "BYXT 应用页不可用: appStatus=" + std::to_string(app_result->status_code));
        return make_error(error.code, to_error(error).message);
    }

    probe_result = probe_academic_portal(http_client, mode);
    if (probe_result && (*probe_result == PortalProbeResult::UndergradReady || *probe_result == PortalProbeResult::GraduateReady)) {
        return {};
    }

    if (probe_result && *probe_result == PortalProbeResult::SsoRequired) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::ReadyCheck,
                                           DownstreamSessionState::SsoRequired,
                                           "BYXT 会话激活后仍需 SSO 认证: appStatus=" + std::to_string(app_result->status_code));
        return make_error(error.code, to_error(error).message);
    }
    if (probe_result) {
        auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                           DownstreamActivationStage::ReadyCheck,
                                           DownstreamSessionState::UnexpectedResponse,
                                           "BYXT 会话激活后未达到可用状态: appStatus=" + std::to_string(app_result->status_code));
        return make_error(error.code, to_error(error).message);
    }
    auto error = make_downstream_error(DownstreamSystemId::Byxt,
                                       DownstreamActivationStage::ReadyCheck,
                                       DownstreamSessionState::Unavailable,
                                       "BYXT 会话激活失败: 无探测响应");
    return make_error(error.code, to_error(error).message);
}

} // namespace UBAANext::Protocol::Byxt
