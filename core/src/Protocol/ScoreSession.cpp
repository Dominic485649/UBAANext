#include <UBAANext/Protocol/ScoreSession.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

namespace UBAANext::Protocol::Score {

namespace {
constexpr const char *kScoreEntryUrl = "https://app.buaa.edu.cn/site/score/index";
}

std::string resolve_url(const std::string &url, ConnectionMode mode) {
    if (mode != ConnectionMode::WebVPN) {
        return url;
    }
    if (url.rfind("https://d.buaa.edu.cn/", 0) == 0) {
        return url;
    }
    return VpnCipher::to_vpn_url(url);
}

void apply_form_headers(HttpRequest &request) {
    request.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
    request.headers["Referer"] = kScoreEntryUrl;
    request.headers["User-Agent"] = "UBAANext/0.4";
}

bool is_session_expired_response(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response);
}

Result<void> ensure_session(IHttpClient &http_client, ConnectionMode mode) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_url(kScoreEntryUrl, mode);
    request.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
    request.headers["User-Agent"] = "UBAANext/0.4";

    auto response = http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "成绩系统会话同步失败: " + response.error().message);
    }
    if (is_session_expired_response(*response)) {
        return make_error(ErrorCode::SessionExpired, "成绩系统会话已过期");
    }
    return {};
}

} // namespace UBAANext::Protocol::Score
