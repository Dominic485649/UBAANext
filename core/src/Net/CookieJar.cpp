/**
 * @file CookieJar.cpp
 * @brief 内存 Cookie 罐的实现
 */

#include <UBAANext/Net/CookieJar.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace UBAANext {

namespace {

std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::string lower_copy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

bool has_control_chars(std::string_view value) {
    return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
        return ch < 0x20 || ch == 0x7f;
    });
}

bool is_valid_cookie_host(std::string_view host) {
    if (host.empty()) {
        return true;
    }
    if (host.find("..") != std::string_view::npos || host.front() == '.' || host.back() == '.') {
        return false;
    }
    if (host.find('.') == std::string_view::npos) {
        return false;
    }
    return std::all_of(host.begin(), host.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '-' || ch == '.';
    });
}

bool domain_matches(std::string_view request_host, std::string_view cookie_host) {
    auto request = lower_copy(request_host);
    auto cookie = lower_copy(cookie_host);
    if (cookie.empty()) {
        return request.empty();
    }
    if (!is_valid_cookie_host(cookie)) {
        return false;
    }
    if (request == cookie) {
        return true;
    }
    if (request.size() <= cookie.size()) {
        return false;
    }
    auto suffix_pos = request.size() - cookie.size();
    return request.substr(suffix_pos) == cookie && request[suffix_pos - 1] == '.';
}

} // namespace

void CookieJar::set_cookie(std::string name, std::string value) {
    set_cookie({}, "/", std::move(name), std::move(value));
}

void CookieJar::set_cookie(std::string host, std::string name, std::string value) {
    set_cookie(std::move(host), "/", std::move(name), std::move(value));
}

void CookieJar::set_cookie(std::string host, std::string path, std::string name, std::string value) {
    host = lower_copy(trim(std::move(host)));
    path = trim(std::move(path));
    if (path.empty()) {
        path = "/";
    }
    name = trim(std::move(name));
    if (name.empty() || !is_valid_cookie_host(host) || has_control_chars(name) ||
        has_control_chars(value) || has_control_chars(path)) {
        return;
    }
    m_cookies[{std::move(host), std::move(path), std::move(name)}] = std::move(value);
}

std::optional<std::string> CookieJar::get_cookie(std::string_view name) const {
    return get_cookie({}, name);
}

std::optional<std::string> CookieJar::get_cookie(std::string_view host, std::string_view name) const {
    for (const auto &[key, value] : m_cookies) {
        if (key.name == name && domain_matches(host, key.host)) {
            return value;
        }
    }
    return std::nullopt;
}

void CookieJar::remove_cookie(std::string_view name) {
    remove_cookie({}, name);
}

void CookieJar::remove_cookie(std::string_view host, std::string_view name) {
    for (auto it = m_cookies.begin(); it != m_cookies.end();) {
        if (it->first.name == name && domain_matches(host, it->first.host)) {
            it = m_cookies.erase(it);
        } else {
            ++it;
        }
    }
}

void CookieJar::clear() {
    m_cookies.clear();
}

std::string CookieJar::to_header() const {
    return to_header({});
}

std::string CookieJar::to_header(std::string_view host) const {
    std::vector<std::pair<CookieKey, std::string>> matches;
    for (const auto &[key, value] : m_cookies) {
        if (domain_matches(host, key.host)) {
            matches.push_back({key, value});
        }
    }
    std::sort(matches.begin(), matches.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.first.path.size() != rhs.first.path.size()) {
            return lhs.first.path.size() > rhs.first.path.size();
        }
        return lhs.first.name < rhs.first.name;
    });

    std::string result;
    bool first = true;
    for (const auto &[key, value] : matches) {
        if (!first) {
            result += "; ";
        }
        result += key.name;
        result += '=';
        result += value;
        first = false;
    }
    return result;
}

std::vector<std::string> CookieJar::serialize() const {
    std::vector<std::string> lines;
    lines.reserve(m_cookies.size());
    for (const auto &[key, value] : m_cookies) {
        lines.push_back(key.host + "\t" + key.path + "\t" + key.name + "\t" + value);
    }
    return lines;
}

void CookieJar::load_serialized_line(const std::string &line) {
    std::vector<std::string> parts;
    std::istringstream ss(line);
    std::string part;
    while (std::getline(ss, part, '\t')) {
        parts.push_back(part);
    }

    if (parts.size() >= 4) {
        set_cookie(std::move(parts[0]), std::move(parts[1]), std::move(parts[2]), std::move(parts[3]));
        return;
    }
    if (parts.size() >= 3) {
        set_cookie(std::move(parts[0]), std::move(parts[1]), std::move(parts[2]));
        return;
    }

    auto eq = line.find('=');
    if (eq != std::string::npos) {
        auto name = trim(line.substr(0, eq));
        auto value = line.substr(eq + 1);
        set_cookie({}, std::move(name), std::move(value));
    }
}

} // namespace UBAANext
