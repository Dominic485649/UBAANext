#include <UBAANext/Protocol/RedirectNavigator.hpp>

#include <UBAANext/Protocol/SessionGuards.hpp>

#include <cstdlib>
#include <regex>
#include <sstream>
#include <utility>

namespace UBAANext::Protocol {

std::string resolve_location(const std::string &base_url, const std::string &location) {
    if (location.rfind("http://", 0) == 0 || location.rfind("https://", 0) == 0) {
        return location;
    }

    std::regex url_re(R"(^([^:]+://[^/]+)(/.*)?$)");
    std::smatch match;
    if (!std::regex_search(base_url, match, url_re)) {
        return location;
    }

    std::string authority = match[1].str();
    std::string path = match.size() > 2 ? match[2].str() : "/";
    auto query = path.find_first_of("?#");
    std::string path_without_query = query == std::string::npos ? path : path.substr(0, query);

    if (location.rfind("//", 0) == 0) {
        auto colon = authority.find(':');
        return authority.substr(0, colon) + ":" + location;
    }
    if (!location.empty() && location.front() == '/') {
        return authority + location;
    }
    if (!location.empty() && (location.front() == '?' || location.front() == '#')) {
        return authority + path_without_query + location;
    }

    auto slash = path_without_query.find_last_of('/');
    std::string base_path = slash == std::string::npos ? "/" : path_without_query.substr(0, slash + 1);
    return authority + base_path + location;
}

std::string extract_query_parameter(const std::string &url, const std::string &name) {
    const auto query_start = url.find_first_of("?#");
    if (query_start == std::string::npos) {
        return {};
    }
    std::size_t pos = query_start + 1;
    while (pos <= url.size()) {
        auto next = url.find_first_of("& #", pos);
        auto pair = url.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        auto eq = pair.find('=');
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

    std::regex re("[?&#]" + name + R"(=([^&#\s]+))");
    std::smatch match;
    if (std::regex_search(url, match, re) && match.size() > 1) {
        return match[1].str();
    }

    std::regex nested_re(R"(([a-zA-Z][a-zA-Z0-9+.-]*%3A%2F%2F[^\s&#]+))", std::regex::icase);
    auto begin = std::sregex_iterator(url.begin(), url.end(), nested_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        auto encoded = (*it)[1].str();
        std::ostringstream decoded;
        for (std::size_t i = 0; i < encoded.size(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.size()) {
                auto hex = encoded.substr(i + 1, 2);
                char *tail = nullptr;
                auto value = std::strtol(hex.c_str(), &tail, 16);
                if (tail && *tail == '\0') {
                    decoded << static_cast<char>(value);
                    i += 2;
                    continue;
                }
            }
            decoded << (encoded[i] == '+' ? ' ' : encoded[i]);
        }
        if (auto value = extract_query_parameter(decoded.str(), name); !value.empty()) {
            return value;
        }
    }
    return {};
}

std::string redact_url_query(const std::string &url) {
    auto query = url.find('?');
    if (query == std::string::npos) {
        return url;
    }
    auto fragment = url.find('#', query);
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
