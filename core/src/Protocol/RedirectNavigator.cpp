#include <UBAANext/Protocol/RedirectNavigator.hpp>

#include <UBAANext/Protocol/SessionGuards.hpp>

#include <cctype>
#include <string_view>
#include <utility>

namespace UBAANext::Protocol {

namespace {

bool is_absolute_http_url(std::string_view value) {
    return value.rfind("http://", 0) == 0 || value.rfind("https://", 0) == 0;
}

bool is_scheme_char(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '+' || ch == '.' || ch == '-';
}

bool is_url_break(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isspace(uch) || ch == '&' || ch == '#' || ch == '"' || ch == '\'' || ch == '<' || ch == '>';
}

int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

bool case_insensitive_equals_at(std::string_view text, std::size_t pos, std::string_view needle) {
    if (pos + needle.size() > text.size()) return false;
    for (std::size_t i = 0; i < needle.size(); ++i) {
        const auto left = static_cast<unsigned char>(text[pos + i]);
        const auto right = static_cast<unsigned char>(needle[i]);
        if (std::tolower(left) != std::tolower(right)) return false;
    }
    return true;
}

bool parameter_name_equals_at(std::string_view text, std::size_t pos, std::string_view name) {
    if (pos + name.size() > text.size()) return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (text[pos + i] != name[i]) return false;
    }
    return true;
}

std::string percent_decode(std::string_view encoded) {
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            const auto hi = hex_value(encoded[i + 1]);
            const auto lo = hex_value(encoded[i + 2]);
            if (hi >= 0 && lo >= 0) {
                decoded.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        decoded.push_back(encoded[i] == '+' ? ' ' : encoded[i]);
    }
    return decoded;
}

std::string extract_delimited_parameter_anywhere(std::string_view text, std::string_view name) {
    for (std::size_t marker = text.find_first_of("?&#"); marker != std::string_view::npos; marker = text.find_first_of("?&#", marker + 1)) {
        const auto name_start = marker + 1;
        const auto value_marker = name_start + name.size();
        if (!parameter_name_equals_at(text, name_start, name) || value_marker >= text.size() || text[value_marker] != '=') {
            continue;
        }
        auto value_end = value_marker + 1;
        while (value_end < text.size() && text[value_end] != '&' && text[value_end] != '#' && !std::isspace(static_cast<unsigned char>(text[value_end]))) {
            ++value_end;
        }
        return std::string(text.substr(value_marker + 1, value_end - value_marker - 1));
    }
    return {};
}

std::string extract_nested_encoded_parameter(std::string_view text, std::string_view name) {
    constexpr std::string_view scheme_separator = "%3A%2F%2F";
    for (std::size_t marker = 0; marker < text.size();) {
        if (!case_insensitive_equals_at(text, marker, scheme_separator)) {
            ++marker;
            continue;
        }

        auto start = marker;
        while (start > 0 && is_scheme_char(text[start - 1])) --start;
        if (start == marker || !std::isalpha(static_cast<unsigned char>(text[start]))) {
            ++marker;
            continue;
        }

        auto end = marker + scheme_separator.size();
        while (end < text.size() && !is_url_break(text[end])) ++end;

        const auto decoded = percent_decode(text.substr(start, end - start));
        if (auto value = extract_query_parameter(decoded, std::string(name)); !value.empty()) return value;
        if (auto value = extract_delimited_parameter_anywhere(decoded, name); !value.empty()) return value;
        marker = end;
    }
    return {};
}

} // namespace

std::string resolve_location(const std::string &base_url, const std::string &location) {
    if (is_absolute_http_url(location)) {
        return location;
    }

    const auto scheme = base_url.find("://");
    if (scheme == std::string::npos) {
        return location;
    }
    const auto authority_end = base_url.find('/', scheme + 3);
    const std::string authority = authority_end == std::string::npos ? base_url : base_url.substr(0, authority_end);
    const std::string path = authority_end == std::string::npos ? "/" : base_url.substr(authority_end);
    const auto query = path.find_first_of("?#");
    const std::string path_without_query = query == std::string::npos ? path : path.substr(0, query);

    if (location.rfind("//", 0) == 0) {
        return base_url.substr(0, scheme) + ":" + location;
    }
    if (!location.empty() && location.front() == '/') {
        return authority + location;
    }
    if (!location.empty() && (location.front() == '?' || location.front() == '#')) {
        return authority + path_without_query + location;
    }

    const auto slash = path_without_query.find_last_of('/');
    const std::string base_path = slash == std::string::npos ? "/" : path_without_query.substr(0, slash + 1);
    return authority + base_path + location;
}

std::string extract_query_parameter(const std::string &url, const std::string &name) {
    const auto query_start = url.find_first_of("?#");
    if (query_start == std::string::npos) {
        return {};
    }
    std::size_t pos = query_start + 1;
    while (pos <= url.size()) {
        const auto next = url.find_first_of("& #", pos);
        const auto pair = url.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        const auto eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == name) {
            return pair.substr(eq + 1);
        }
        if (next == std::string::npos || url[next] == '#' || url[next] == ' ') {
            break;
        }
        pos = next + 1;
    }
    return {};
}

std::string extract_query_parameter_anywhere(const std::string &url, const std::string &name) {
    if (auto value = extract_query_parameter(url, name); !value.empty()) {
        return value;
    }
    if (auto value = extract_delimited_parameter_anywhere(url, name); !value.empty()) {
        return value;
    }
    return extract_nested_encoded_parameter(url, name);
}

std::string redact_url_query(const std::string &url) {
    const auto query = url.find('?');
    if (query == std::string::npos) {
        return url;
    }
    const auto fragment = url.find('#', query);
    return url.substr(0, query) + "?<redacted>" + (fragment == std::string::npos ? std::string{} : url.substr(fragment));
}

void disable_transport_redirects(HttpRequest &request) {
    request.redirect.follow_redirects = false;
    request.redirect.max_redirects = 0;
    request.redirect.expose_location_header = true;
}

Result<RedirectNavigationResult> follow_redirects(IHttpClient &http_client,
                                                  ConnectionMode mode,
                                                  std::string initial_url,
                                                  DownstreamSystemId system,
                                                  UrlResolver resolve_url,
                                                  RequestConfigurer configure_request,
                                                  RedirectStepObserver observe_step,
                                                  int max_redirects) {
    std::string current_url = std::move(initial_url);
    HttpResponse current_response;

    for (int redirects = 0; redirects <= max_redirects; ++redirects) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_url ? resolve_url(current_url, mode) : current_url;
        request.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";
        request.headers["User-Agent"] = "UBAANext/0.4";
        disable_transport_redirects(request);
        if (configure_request) {
            configure_request(request);
        }

        auto response = http_client.send(request);
        if (!response) {
            auto err = make_downstream_error(system,
                                             DownstreamActivationStage::RedirectFollow,
                                             DownstreamSessionState::Unavailable,
                                             "下游系统重定向请求失败: " + response.error().message,
                                             redact_url_query(current_url));
            return make_error(err.code, to_error(err).message);
        }

        current_response = *response;
        RedirectStep step{current_url, request.url, current_response};
        if (observe_step) {
            auto observed = observe_step(step);
            if (!observed) {
                return make_error(observed.error().code, observed.error().message);
            }
        }

        if (current_response.status_code < 300 || current_response.status_code >= 400) {
            RedirectNavigationResult result;
            result.final_url = std::move(current_url);
            result.final_response = std::move(current_response);
            return result;
        }

        auto location = header_value(current_response, "Location");
        if (location.empty()) {
            auto err = make_downstream_error(system,
                                             DownstreamActivationStage::RedirectFollow,
                                             DownstreamSessionState::ProtocolError,
                                             "下游系统重定向缺少 Location",
                                             redact_url_query(current_url));
            return make_error(err.code, to_error(err).message);
        }
        current_url = resolve_location(current_url, location);
    }

    auto err = make_downstream_error(system,
                                     DownstreamActivationStage::RedirectFollow,
                                     DownstreamSessionState::Unavailable,
                                     "下游系统重定向次数过多",
                                     redact_url_query(current_url));
    return make_error(err.code, to_error(err).message);
}

} // namespace UBAANext::Protocol
