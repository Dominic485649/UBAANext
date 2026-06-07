#include <UBAANext/Platform/Curl/CurlHttpClient.hpp>

#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>
#include <UBAANext/Platform/Curl/CurlRuntime.hpp>

#include <curl/curl.h>

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace UBAANext::Platform::Curl {
namespace {

size_t write_body(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *body = static_cast<std::string *>(userdata);
    body->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string trim(std::string value) {
    auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool header_name_equals(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

struct HeaderCapture {
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::string> set_cookie_values;
};

size_t write_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *capture = static_cast<HeaderCapture *>(userdata);
    std::string line(ptr, size * nmemb);
    if (!line.empty() && line.back() == '\n') line.pop_back();
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto colon = line.find(':');
    if (colon == std::string::npos) {
        return size * nmemb;
    }
    auto name = trim(line.substr(0, colon));
    auto value = trim(line.substr(colon + 1));
    if (name.empty()) {
        return size * nmemb;
    }
    if (header_name_equals(name, "Set-Cookie")) {
        capture->set_cookie_values.push_back(value);
    }
    auto existing = capture->headers.find(name);
    if (existing == capture->headers.end()) {
        capture->headers.emplace(std::move(name), std::move(value));
    } else {
        existing->second += "\n" + value;
    }
    return size * nmemb;
}

const char *method_name(HttpMethod method) {
    switch (method) {
    case HttpMethod::Get: return "GET";
    case HttpMethod::Post: return "POST";
    case HttpMethod::Put: return "PUT";
    case HttpMethod::Delete: return "DELETE";
    }
    return "GET";
}

std::string redact_url_if_needed(const HttpRequest &request) {
    if (!request.transport.redact_url_query_in_errors) {
        return request.url;
    }
    auto query = request.url.find('?');
    if (query == std::string::npos) {
        return request.url;
    }
    return request.url.substr(0, query) + "?<redacted>";
}

std::string extract_host(const std::string &url) {
    auto scheme = url.find("://");
    auto start = scheme == std::string::npos ? 0 : scheme + 3;
    auto end = url.find_first_of("/?#", start);
    auto authority = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    auto at = authority.rfind('@');
    if (at != std::string::npos) {
        authority = authority.substr(at + 1);
    }
    if (!authority.empty() && authority.front() == '[') {
        auto close = authority.find(']');
        return close == std::string::npos ? authority : authority.substr(1, close - 1);
    }
    auto colon = authority.find(':');
    return colon == std::string::npos ? authority : authority.substr(0, colon);
}

void apply_set_cookie(CookieJar &cookies, const std::string &request_host, const std::string &value) {
    auto eq = value.find('=');
    if (eq == std::string::npos) {
        return;
    }
    auto semi = value.find(';');
    auto name = trim(value.substr(0, eq));
    auto cookie_value = semi == std::string::npos ? value.substr(eq + 1) : value.substr(eq + 1, semi - eq - 1);
    auto cookie_host = request_host;
    std::string cookie_path = "/";
    bool expired = cookie_value.empty();

    if (semi != std::string::npos) {
        std::istringstream attrs(value.substr(semi + 1));
        std::string attr;
        while (std::getline(attrs, attr, ';')) {
            attr = trim(std::move(attr));
            auto attr_eq = attr.find('=');
            auto attr_name = attr_eq == std::string::npos ? attr : attr.substr(0, attr_eq);
            for (auto &ch : attr_name) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            if (attr_eq == std::string::npos) {
                continue;
            }
            auto attr_value = attr.substr(attr_eq + 1);
            if (attr_name == "domain") {
                cookie_host = attr_value;
                if (!cookie_host.empty() && cookie_host.front() == '.') {
                    cookie_host.erase(cookie_host.begin());
                }
            } else if (attr_name == "path") {
                cookie_path = attr_value;
            } else if (attr_name == "max-age" && attr_value == "0") {
                expired = true;
            } else if (attr_name == "expires" && attr_value.find("1970") != std::string::npos) {
                expired = true;
            }
        }
    }

    if (expired) {
        cookies.remove_cookie(cookie_host, name);
    } else {
        cookies.set_cookie(std::move(cookie_host), std::move(cookie_path), std::move(name), std::move(cookie_value));
    }
}

Result<void> apply_redirect_options(CURL *curl, const HttpRequest &request) {
    const auto &redirect = request.redirect;
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, redirect.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(redirect.max_redirects));
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, redirect.restrict_to_http_https ? "http,https" : "all");
    curl_easy_setopt(curl, CURLOPT_POSTREDIR,
                     redirect.post_policy == RedirectPostPolicy::PreserveMethod ? CURL_REDIR_POST_ALL : 0L);
    return {};
}

} // namespace

CurlHttpClient::CurlHttpClient(UBAANext::CookieJar &cookies)
    : m_live_cookies(&cookies) {}

UBAANext::CookieJar &CurlHttpClient::live_cookies() {
    return *m_live_cookies;
}

Result<HttpResponse> CurlHttpClient::send(const HttpRequest &request) {
    CurlRuntime runtime;
    if (!runtime.ok()) {
        return make_error(runtime.error().code, runtime.error().message);
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        return make_error(ErrorCode::UnsupportedNetwork, "Curl easy handle 初始化失败");
    }

    std::string response_body;
    HeaderCapture header_capture;
    char error_buffer[CURL_ERROR_SIZE]{};
    curl_slist *header_list = nullptr;

    auto cleanup = [&] {
        if (header_list) {
            curl_slist_free_all(header_list);
        }
        curl_easy_cleanup(curl);
    };

    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_name(request.method));
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_capture);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (request.transport.connect_timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(request.transport.connect_timeout_ms));
    }
    if (request.transport.request_timeout_ms > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(request.transport.request_timeout_ms));
    }
    if (!request.transport.proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, request.transport.proxy.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.transport.tls_verify_peer ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.transport.tls_verify_host ? 2L : 0L);

    auto redirect = apply_redirect_options(curl, request);
    if (!redirect) {
        cleanup();
        return make_error(redirect.error().code, redirect.error().message);
    }

    const auto request_host = extract_host(request.url);
    auto cookie_header = m_live_cookies->to_header(request_host);
    bool caller_supplied_cookie = false;
    for (const auto &[name, value] : request.headers) {
        if (name == "Cookie" || name == "cookie") {
            caller_supplied_cookie = true;
        }
        const auto header = name + ": " + value;
        auto *next = curl_slist_append(header_list, header.c_str());
        if (!next) {
            cleanup();
            return make_error(ErrorCode::NetworkError, "Curl 请求头构造失败");
        }
        header_list = next;
    }
    if (!caller_supplied_cookie && !cookie_header.empty()) {
        const auto header = "Cookie: " + cookie_header;
        auto *next = curl_slist_append(header_list, header.c_str());
        if (!next) {
            cleanup();
            return make_error(ErrorCode::NetworkError, "Curl Cookie 请求头构造失败");
        }
        header_list = next;
    }
    if (header_list) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    if (!request.body.empty() || request.method == HttpMethod::Post || request.method == HttpMethod::Put) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    }

    auto code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        auto error = map_curl_error(code, error_buffer);
        auto url = redact_url_if_needed(request);
        cleanup();
        return make_error(error.error.code, error.error.message + " url=" + url);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    for (const auto &set_cookie : header_capture.set_cookie_values) {
        apply_set_cookie(*m_live_cookies, request_host, set_cookie);
    }
    cleanup();

    HttpResponse response;
    response.status_code = static_cast<int>(status);
    response.headers = std::move(header_capture.headers);
    response.body = std::move(response_body);
    return response;
}

} // namespace UBAANext::Platform::Curl
