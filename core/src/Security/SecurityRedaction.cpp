#include <UBAANext/Security/SecurityRedaction.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace UBAANext {
namespace Security {
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

[[nodiscard]] bool is_url_end(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) || ch == '"' || ch == '\'' || ch == '<' || ch == '>' || ch == ')' || ch == ',';
}

[[nodiscard]] size_t find_ci(std::string_view text, std::string_view needle, size_t pos = 0) {
    if (needle.empty() || needle.size() > text.size()) return std::string::npos;
    for (size_t i = pos; i + needle.size() <= text.size(); ++i) {
        if (starts_with_ci(text, i, needle)) return i;
    }
    return std::string::npos;
}

void redact_url_queries(std::string &text) {
    size_t pos = 0;
    while (pos < text.size()) {
        const auto http = text.find("http://", pos);
        const auto https = text.find("https://", pos);
        size_t start = std::string::npos;
        if (http == std::string::npos) start = https;
        else if (https == std::string::npos) start = http;
        else start = std::min(http, https);
        if (start == std::string::npos) break;

        size_t url_end = start;
        while (url_end < text.size() && !is_url_end(text[url_end])) ++url_end;
        const auto query = text.find('?', start);
        if (query == std::string::npos || query >= url_end) {
            pos = url_end;
            continue;
        }
        const auto fragment = text.find('#', query + 1);
        const size_t query_end = fragment != std::string::npos && fragment < url_end ? fragment : url_end;
        text.replace(query + 1, query_end - query - 1, "[REDACTED]");
        pos = query + 11;
    }
}

void redact_drive_paths(std::string &text) {
    size_t pos = 0;
    while (pos + 2 < text.size()) {
        if (!std::isalpha(static_cast<unsigned char>(text[pos])) || text[pos + 1] != ':' || (text[pos + 2] != '/' && text[pos + 2] != '\\')) {
            ++pos;
            continue;
        }
        size_t end = pos + 3;
        while (end < text.size() && !is_value_end(text[end])) ++end;
        text.replace(pos, end - pos, "[REDACTED]");
        pos += 10;
    }
}

void redact_prefixed_paths(std::string &text, std::string_view prefix) {
    size_t pos = 0;
    while ((pos = text.find(prefix, pos)) != std::string::npos) {
        size_t end = pos + prefix.size();
        while (end < text.size() && !is_value_end(text[end])) ++end;
        text.replace(pos, end - pos, "[REDACTED]");
        pos += 10;
    }
}

void redact_html_fragments(std::string &text) {
    std::array<std::string_view, 3> markers = {"<!doctype html", "<html", "<form"};
    size_t pos = std::string::npos;
    for (const auto marker : markers) {
        const auto found = find_ci(text, marker);
        if (found != std::string::npos && (pos == std::string::npos || found < pos)) pos = found;
    }
    if (pos != std::string::npos) text.replace(pos, text.size() - pos, "[REDACTED]");
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
    redact_url_queries(redacted);

    constexpr std::array<std::string_view, 29> keys = {
        "username", "account", "student_id", "studentId", "password", "passwd", "pwd",
        "token", "ticket", "cas", "execution", "session", "session_id", "captcha", "验证码",
        "cookie", "authorization", "cgauthorization", "photo_path", "path", "filename", "file",
        "lock_code", "lockCode", "lockcode", "booking_id", "bookingId", "place", "location",
    };
    for (const auto key : keys) {
        redact_key_values(redacted, key);
    }

    redact_header_values(redacted, "Cookie");
    redact_header_values(redacted, "Set-Cookie");
    redact_header_values(redacted, "Authorization");
    redact_header_values(redacted, "cgAuthorization");
    redact_drive_paths(redacted);
    redact_prefixed_paths(redacted, "/data/");
    redact_prefixed_paths(redacted, "/storage/");
    redact_prefixed_paths(redacted, "/sdcard/");
    redact_html_fragments(redacted);
    return redacted;
}

} // namespace Security
} // namespace UBAANext
