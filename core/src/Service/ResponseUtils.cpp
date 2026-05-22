#include <UBAANext/Service/ResponseUtils.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>

#include <algorithm>
#include <cctype>

namespace UBAANext {
namespace ServiceResponse {
namespace {

[[nodiscard]] std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

} // namespace

bool is_session_expired_message(const std::string &message) {
    return Protocol::has_session_expired_marker(message);
}

bool is_session_expired_response(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response, {}, true);
}

bool envelope_ok(const nlohmann::json &json) {
    if (!json.contains("code")) return true;
    const auto &code = json["code"];
    if (code.is_number_integer()) {
        const auto value = code.get<int>();
        return value == 0 || value == 1 || value == 200;
    }
    if (code.is_string()) {
        const auto value = lower_copy(code.get<std::string>());
        return value == "0" || value == "1" || value == "200" || value == "success" || value == "ok";
    }
    if (code.is_boolean()) return code.get<bool>();
    return false;
}

std::string envelope_message(const nlohmann::json &json, const std::string &fallback) {
    for (const auto *key : {"message", "msg", "errmsg", "ERRMSG", "error"}) {
        if (!json.contains(key) || json[key].is_null()) continue;
        if (json[key].is_string()) return json[key].get<std::string>();
        if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    }
    return fallback;
}

nlohmann::json envelope_data(const nlohmann::json &json) {
    if (json.contains("data")) return json["data"];
    if (json.contains("result")) return json["result"];
    if (json.contains("content")) return json["content"];
    if (json.contains("list")) return json["list"];
    return json;
}

Result<nlohmann::json> parse_json_response(const HttpResponse &response, const std::string &context) {
    if (is_session_expired_response(response)) {
        return make_error(ErrorCode::SessionExpired, context + "会话已过期，请重新登录");
    }
    if (response.status_code < 200 || response.status_code >= 300) {
        return make_error(ErrorCode::NetworkError, context + "请求返回: " + std::to_string(response.status_code));
    }
    auto json = nlohmann::json::parse(response.body, nullptr, false);
    if (json.is_discarded()) {
        return make_error(ErrorCode::ParseError, "解析" + context + "JSON 失败");
    }
    const auto message = envelope_message(json, context + "请求失败");
    if (is_session_expired_message(message)) {
        return make_error(ErrorCode::SessionExpired, message);
    }
    if (!envelope_ok(json)) {
        return make_error(ErrorCode::NetworkError, message);
    }
    return envelope_data(json);
}

} // namespace ServiceResponse
} // namespace UBAANext
