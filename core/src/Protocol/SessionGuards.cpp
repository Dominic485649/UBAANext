#include <UBAANext/Protocol/SessionGuards.hpp>

#include <algorithm>
#include <cctype>

namespace UBAANext::Protocol {

namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

std::string header_value(const HttpResponse &response, const std::string &name) {
    const auto wanted = to_lower(name);
    for (const auto &[key, value] : response.headers) {
        if (to_lower(key) == wanted) return value;
    }
    return {};
}

bool has_sso_login_marker(const std::string &text) {
    return text.find("统一身份认证") != std::string::npos ||
           text.find("input name=\"execution\"") != std::string::npos ||
           text.find("name='execution'") != std::string::npos ||
           text.find("sso.buaa.edu.cn") != std::string::npos;
}

bool has_session_expired_marker(const std::string &text) {
    return has_sso_login_marker(text) ||
           text.find("请重新登录") != std::string::npos ||
           text.find("登录失效") != std::string::npos ||
           text.find("会话已过期") != std::string::npos ||
           text.find("您的会话已经过期") != std::string::npos ||
           text.find("会话已经过期") != std::string::npos ||
           text.find("\"url\":\"/login\"") != std::string::npos ||
           text.find("\"url\": \"/login\"") != std::string::npos;
}

bool is_session_expired_response(const HttpResponse &response) {
    return response.status_code == 401 || response.status_code == 403 ||
           has_session_expired_marker(header_value(response, "Location")) ||
           has_session_expired_marker(response.body);
}

} // namespace UBAANext::Protocol
