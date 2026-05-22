#include "SecurityRedaction.hpp"

#include <array>
#include <cctype>
#include <string_view>

namespace UBAANextCli {
namespace {

[[nodiscard]] char lower_ascii(char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

[[nodiscard]] bool starts_with_ci(std::string_view text, size_t pos, std::string_view prefix) {
    if (pos + prefix.size() > text.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (lower_ascii(text[pos + i]) != lower_ascii(prefix[i])) return false;
    }
    return true;
}

[[nodiscard]] bool is_key_char(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-';
}

[[nodiscard]] bool is_value_end(char ch) {
    return ch == '&' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '"' || ch == '\'' || ch == ',' || ch == '}' || ch == ']';
}

[[nodiscard]] bool is_sensitive_key_at(std::string_view text, size_t pos, std::string_view key) {
    if (!starts_with_ci(text, pos, key)) return false;
    if (pos > 0 && is_key_char(text[pos - 1])) return false;
    const size_t after = pos + key.size();
    return after == text.size() || !is_key_char(text[after]);
}

void redact_key_values(std::string &text, std::string_view key) {
    size_t pos = 0;
    while (pos < text.size()) {
        if (!is_sensitive_key_at(text, pos, key)) {
            ++pos;
            continue;
        }

        size_t cursor = pos + key.size();
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) ++cursor;
        if (cursor >= text.size() || (text[cursor] != '=' && text[cursor] != ':')) {
            pos = cursor;
            continue;
        }
        ++cursor;
        while (cursor < text.size() && std::isspace(static_cast<unsigned char>(text[cursor]))) ++cursor;

        const size_t value_start = cursor;
        if (cursor < text.size() && (text[cursor] == '"' || text[cursor] == '\'')) {
            const char quote = text[cursor++];
            const size_t content_start = cursor;
            while (cursor < text.size() && text[cursor] != quote) ++cursor;
            text.replace(content_start, cursor - content_start, "[REDACTED]");
            pos = content_start + 10;
            continue;
        }

        while (cursor < text.size() && !is_value_end(text[cursor])) ++cursor;
        if (cursor > value_start) {
            text.replace(value_start, cursor - value_start, "[REDACTED]");
            pos = value_start + 10;
        } else {
            pos = cursor + 1;
        }
    }
}

void redact_header_values(std::string &text, std::string_view header) {
    size_t pos = 0;
    while (pos < text.size()) {
        if (!starts_with_ci(text, pos, header)) {
            ++pos;
            continue;
        }
        const size_t after = pos + header.size();
        if (after >= text.size() || text[after] != ':') {
            pos = after;
            continue;
        }
        size_t value_start = after + 1;
        while (value_start < text.size() && std::isspace(static_cast<unsigned char>(text[value_start]))) ++value_start;
        size_t value_end = value_start;
        while (value_end < text.size() && text[value_end] != '\r' && text[value_end] != '\n') ++value_end;
        text.replace(value_start, value_end - value_start, "[REDACTED]");
        pos = value_start + 10;
    }
}

} // namespace

std::string redact_proxy_url(const std::string &proxy) {
    const auto scheme = proxy.find("://");
    if (scheme == std::string::npos) return proxy;
    const size_t authority_start = scheme + 3;
    const auto at = proxy.find('@', authority_start);
    if (at == std::string::npos) return proxy;
    const auto authority_end = proxy.find_first_of("/?#", authority_start);
    if (authority_end != std::string::npos && at > authority_end) return proxy;

    std::string redacted = proxy;
    redacted.replace(authority_start, at - authority_start, "[REDACTED]");
    return redacted;
}

std::string redact_sensitive_text(const std::string &text) {
    std::string redacted = text;
    redacted = redact_proxy_url(redacted);

    constexpr std::array<std::string_view, 15> keys = {
        "password", "passwd", "pwd", "token", "ticket", "cas", "execution", "session", "session_id", "captcha", "验证码", "cookie", "authorization", "cgauthorization", "photo_path",
    };
    for (const auto key : keys) {
        redact_key_values(redacted, key);
    }

    redact_header_values(redacted, "Cookie");
    redact_header_values(redacted, "Set-Cookie");
    redact_header_values(redacted, "Authorization");
    redact_header_values(redacted, "cgAuthorization");
    return redacted;
}

} // namespace UBAANextCli
