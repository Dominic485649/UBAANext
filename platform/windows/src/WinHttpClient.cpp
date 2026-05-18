/**
 * @file WinHttpClient.cpp
 * @brief 基于 WinHTTP 的 HTTP 客户端实现
 */

#include <UBAANext/Platform/Windows/WinHttpClient.hpp>

#include <dpapi.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

namespace UBAANext {

namespace {

std::string protect_bytes(const std::string &plain) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.data()));
    in.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"UBAANext cookies", nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }

    std::string encrypted(reinterpret_cast<char *>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return encrypted;
}

std::string unprotect_bytes(const std::string &encrypted) {
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(encrypted.data()));
    in.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        return {};
    }

    std::string plain(reinterpret_cast<char *>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return plain;
}

} // namespace

WinHttpClient::WinHttpClient(const WinHttpConfig &config) : m_config(config) {
    init_session();
}

WinHttpClient::~WinHttpClient() {
    if (m_session) {
        WinHttpCloseHandle(m_session);
        m_session = nullptr;
    }
}

bool WinHttpClient::init_session() {
    m_session = WinHttpOpen(
        to_wstring(m_config.user_agent).c_str(),
        m_config.proxy.empty()
            ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
            : WINHTTP_ACCESS_TYPE_NAMED_PROXY,
        m_config.proxy.empty() ? WINHTTP_NO_PROXY_NAME : to_wstring(m_config.proxy).c_str(),
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!m_session) return false;

    // 在 session 级别禁用自动重定向
    if (!m_config.follow_redirects) {
        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(m_session, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
    }

    return true;
}

std::wstring WinHttpClient::to_wstring(const std::string &s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), result.data(), len);
    return result;
}

std::string WinHttpClient::from_wstring(const std::wstring &ws) {
    if (ws.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), result.data(), len, nullptr, nullptr);
    return result;
}

Result<HttpResponse> WinHttpClient::send(const HttpRequest &request) {
    if (!m_session) {
        return make_error(ErrorCode::NetworkError, "WinHTTP 会话未初始化");
    }

    // 解析 URL
    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = 1;
    urlComp.dwHostNameLength = 1;
    urlComp.dwUrlPathLength = 1;
    urlComp.dwExtraInfoLength = 1;

    std::wstring wurl = to_wstring(request.url);
    if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &urlComp)) {
        return make_error(ErrorCode::NetworkError, "URL 解析失败: " + request.url);
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    std::wstring extra(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    INTERNET_PORT port = urlComp.nPort;
    bool is_https = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    // 打开连接
    HINTERNET hConnect = WinHttpConnect(m_session, host.c_str(), port, 0);
    if (!hConnect) {
        return make_error(ErrorCode::NetworkError, "WinHttpConnect 失败: " + std::to_string(GetLastError()));
    }

    // 确定 HTTP 方法
    const wchar_t *method;
    switch (request.method) {
    case HttpMethod::Get:    method = L"GET"; break;
    case HttpMethod::Post:   method = L"POST"; break;
    case HttpMethod::Put:    method = L"PUT"; break;
    case HttpMethod::Delete: method = L"DELETE"; break;
    default:                 method = L"GET"; break;
    }

    std::wstring full_path = path + extra;
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, method, full_path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return make_error(ErrorCode::NetworkError, "WinHttpOpenRequest 失败: " + std::to_string(GetLastError()));
    }

    // 设置超时
    WinHttpSetTimeouts(hRequest, m_config.connect_timeout_ms, m_config.connect_timeout_ms,
                       m_config.request_timeout_ms, m_config.request_timeout_ms);

    DWORD disable_flags = WINHTTP_DISABLE_COOKIES;
    if (!request.redirect.follow_redirects) {
        disable_flags |= WINHTTP_DISABLE_REDIRECTS;
    }
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DISABLE_FEATURE, &disable_flags, sizeof(disable_flags));

    if (!request.redirect.follow_redirects) {
        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));
    }

    // 构建请求头
    std::wstring headers;
    for (const auto &[k, v] : request.headers) {
        headers += to_wstring(k) + L": " + to_wstring(v) + L"\r\n";
    }

    // 添加当前主机的 Cookie
    auto request_host = from_wstring(host);
    std::string cookie_str = m_cookies.to_header(request_host);
    if (request_host == "d.buaa.edu.cn") {
        auto gateway_cookies = m_cookies.to_header({});
        if (!gateway_cookies.empty()) {
            if (!cookie_str.empty()) {
                cookie_str += "; ";
            }
            cookie_str += gateway_cookies;
        }
    }
    if (!cookie_str.empty()) {
        headers += L"Cookie: " + to_wstring(cookie_str) + L"\r\n";
    }

    // 发送请求
    const void *body_ptr = request.body.empty() ? WINHTTP_NO_REQUEST_DATA : request.body.c_str();
    DWORD body_len = static_cast<DWORD>(request.body.size());

    BOOL ok = WinHttpSendRequest(
        hRequest,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        static_cast<DWORD>(headers.size()),
        const_cast<void *>(body_ptr), body_len, body_len, 0);

    if (!ok) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return make_error(ErrorCode::NetworkError, "WinHttpSendRequest 失败: " + std::to_string(err));
    }

    // 接收响应
    ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (!ok) {
        DWORD err = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return make_error(ErrorCode::NetworkError, "WinHttpReceiveResponse 失败: " + std::to_string(err));
    }

    // 读取状态码
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

    // 读取所有响应头，稍后填入 HttpResponse.headers 并解析 Set-Cookie
    std::unordered_map<std::string, std::string> response_headers;
    DWORD all_headers_size = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &all_headers_size,
                        WINHTTP_NO_HEADER_INDEX);
    if (all_headers_size > 0) {
        std::wstring all_headers(static_cast<size_t>(all_headers_size / sizeof(wchar_t)) + 1, L'\0');
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX, all_headers.data(), &all_headers_size,
                                WINHTTP_NO_HEADER_INDEX)) {
            std::string headers_str = from_wstring(all_headers);
            std::istringstream lines(headers_str);
            std::string line;
            while (std::getline(lines, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                auto colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                std::string name = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                auto start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    value = value.substr(start);
                }

                if (name == "Set-Cookie" || name == "set-cookie") {
                    auto eq = value.find('=');
                    auto semi = value.find(';');
                    if (eq != std::string::npos) {
                        std::string cookie_name = value.substr(0, eq);
                        std::string cookie_value = (semi != std::string::npos)
                            ? value.substr(eq + 1, semi - eq - 1)
                            : value.substr(eq + 1);
                        std::string cookie_host = from_wstring(host);
                        std::string cookie_path = "/";
                        bool expired = cookie_value.empty();
                        if (semi != std::string::npos) {
                            std::string attrs = value.substr(semi + 1);
                            std::istringstream attr_stream(attrs);
                            std::string attr;
                            while (std::getline(attr_stream, attr, ';')) {
                                auto attr_start = attr.find_first_not_of(" \t");
                                if (attr_start != std::string::npos) {
                                    attr = attr.substr(attr_start);
                                }
                                auto attr_eq = attr.find('=');
                                std::string attr_name = attr_eq == std::string::npos ? attr : attr.substr(0, attr_eq);
                                for (auto &ch : attr_name) {
                                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                                }
                                if (attr_eq == std::string::npos) {
                                    continue;
                                }
                                std::string attr_value = attr.substr(attr_eq + 1);
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
                            m_cookies.remove_cookie(cookie_host, cookie_name);
                        } else {
                            m_cookies.set_cookie(std::move(cookie_host), std::move(cookie_path), std::move(cookie_name), std::move(cookie_value));
                        }
                    }
                }

                auto existing = response_headers.find(name);
                if (existing == response_headers.end()) {
                    response_headers.emplace(std::move(name), std::move(value));
                } else {
                    existing->second += "\n" + value;
                }
            }
        }
    }

    // 读取响应体
    std::string body;
    DWORD bytes_available = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0) {
        std::string chunk(static_cast<size_t>(bytes_available), '\0');
        DWORD bytes_read = 0;
        WinHttpReadData(hRequest, chunk.data(), bytes_available, &bytes_read);
        body.append(chunk.data(), bytes_read);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    HttpResponse response;
    response.status_code = static_cast<int>(status_code);
    response.headers = std::move(response_headers);
    response.body = std::move(body);

    return response;
}

void WinHttpClient::save_cookies(const std::string &path) {
    std::string plain;
    for (const auto &line : m_cookies.serialize()) {
        plain += line;
        plain += '\n';
    }

    auto encrypted = protect_bytes(plain);
    if (encrypted.empty() && !plain.empty()) {
        return;
    }

    auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;
    f.write(encrypted.data(), static_cast<std::streamsize>(encrypted.size()));
}

void WinHttpClient::load_cookies(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return;

    std::string encrypted((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string plain = unprotect_bytes(encrypted);
    if (plain.empty()) {
        plain = std::move(encrypted);
    }

    std::istringstream lines(plain);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        m_cookies.load_serialized_line(line);
    }
}

} // namespace UBAANext
