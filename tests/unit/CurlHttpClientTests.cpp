#include <UBAANext/Platform/Curl/CurlErrorMapper.hpp>
#include <UBAANext/Platform/Curl/CurlHttpClient.hpp>
#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalid_socket = INVALID_SOCKET;
void close_socket(SocketHandle socket) { closesocket(socket); }
#else
using SocketHandle = int;
constexpr SocketHandle invalid_socket = -1;
void close_socket(SocketHandle socket) { close(socket); }
#endif

struct SocketRuntime {
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data{};
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
        ok = true;
#endif
    }

    ~SocketRuntime() {
#ifdef _WIN32
        if (ok) WSACleanup();
#endif
    }

    bool ok = false;
};

std::string header_value(const std::string &request, const std::string &name) {
    std::istringstream lines(request);
    std::string line;
    const std::string prefix = name + ":";
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= prefix.size() && line.compare(0, prefix.size(), prefix) == 0) {
            auto start = line.find_first_not_of(" \t", prefix.size());
            return start == std::string::npos ? std::string{} : line.substr(start);
        }
    }
    return {};
}

std::size_t content_length(const std::string &request) {
    const auto value = header_value(request, "Content-Length");
    if (value.empty()) return 0;
    return static_cast<std::size_t>(std::stoul(value));
}

std::string body_from_request(const std::string &request) {
    const auto split = request.find("\r\n\r\n");
    if (split == std::string::npos) return {};
    return request.substr(split + 4);
}

class LocalHttpServer {
public:
    explicit LocalHttpServer(std::string response) : m_response(std::move(response)) {
        REQUIRE(m_runtime.ok);
        m_server = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        REQUIRE(m_server != invalid_socket);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        REQUIRE(::bind(m_server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        REQUIRE(::listen(m_server, 1) == 0);

        sockaddr_in bound{};
#ifdef _WIN32
        int len = sizeof(bound);
#else
        socklen_t len = sizeof(bound);
#endif
        REQUIRE(::getsockname(m_server, reinterpret_cast<sockaddr *>(&bound), &len) == 0);
        m_port = ntohs(bound.sin_port);
        m_thread = std::thread([this] { serve_once(); });
    }

    ~LocalHttpServer() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
        if (m_server != invalid_socket) {
            close_socket(m_server);
        }
    }

    std::string url(const std::string &path) const {
        return "http://127.0.0.1:" + std::to_string(m_port) + path;
    }

    const std::string &request_text() const { return m_request; }

private:
    void serve_once() {
        SocketHandle client = ::accept(m_server, nullptr, nullptr);
        if (client == invalid_socket) return;

        std::string request;
        char buffer[1024]{};
        while (request.find("\r\n\r\n") == std::string::npos) {
#ifdef _WIN32
            int n = ::recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
            ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
#endif
            if (n <= 0) break;
            request.append(buffer, static_cast<std::size_t>(n));
        }
        auto expected_body = content_length(request);
        while (body_from_request(request).size() < expected_body) {
#ifdef _WIN32
            int n = ::recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
            ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
#endif
            if (n <= 0) break;
            request.append(buffer, static_cast<std::size_t>(n));
        }
        m_request = std::move(request);
#ifdef _WIN32
        ::send(client, m_response.data(), static_cast<int>(m_response.size()), 0);
#else
        ::send(client, m_response.data(), m_response.size(), 0);
#endif
        close_socket(client);
    }

    SocketRuntime m_runtime;
    SocketHandle m_server = invalid_socket;
    unsigned short m_port = 0;
    std::string m_response;
    std::string m_request;
    std::thread m_thread;
};

} // namespace

TEST_CASE("CurlErrorMapper 映射 timeout 和 TLS 错误", "[curl][http]") {
    auto timeout = UBAANext::Platform::Curl::map_curl_error(CURLE_OPERATION_TIMEDOUT, "timeout");
    CHECK(timeout.error.code == UBAANext::ErrorCode::Timeout);

    auto tls = UBAANext::Platform::Curl::map_curl_error(CURLE_PEER_FAILED_VERIFICATION, "cert");
    CHECK(tls.error.code == UBAANext::ErrorCode::TlsError);
}

TEST_CASE("CurlHttpClient 发送 GET 并读取状态头和响应体", "[curl][http]") {
    LocalHttpServer server("HTTP/1.1 201 Created\r\nX-Test: yes\r\nContent-Length: 5\r\n\r\nhello");
    UBAANext::Platform::Curl::CurlHttpClient client;
    UBAANext::HttpRequest request;
    request.url = server.url("/hello?secret=1");
    request.headers["X-Client"] = "curl-test";
    request.transport.request_timeout_ms = 5000;

    auto response = client.send(request);

    REQUIRE(response.has_value());
    CHECK(response->status_code == 201);
    CHECK(response->body == "hello");
    REQUIRE(response->headers.find("X-Test") != response->headers.end());
    CHECK(response->headers["X-Test"] == "yes");
    CHECK(server.request_text().find("GET /hello?secret=1 HTTP/1.1") != std::string::npos);
    CHECK(server.request_text().find("X-Client: curl-test") != std::string::npos);
}

TEST_CASE("CurlHttpClient 发送 POST body", "[curl][http]") {
    LocalHttpServer server("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
    UBAANext::Platform::Curl::CurlHttpClient client;
    UBAANext::HttpRequest request;
    request.method = UBAANext::HttpMethod::Post;
    request.url = server.url("/submit");
    request.headers["Content-Type"] = "application/x-www-form-urlencoded";
    request.body = "a=1&b=2";
    request.transport.request_timeout_ms = 5000;

    auto response = client.send(request);

    REQUIRE(response.has_value());
    CHECK(response->status_code == 200);
    CHECK(response->body == "ok");
    CHECK(server.request_text().find("POST /submit HTTP/1.1") != std::string::npos);
    CHECK(server.request_text().find("Content-Type: application/x-www-form-urlencoded") != std::string::npos);
    CHECK(body_from_request(server.request_text()) == "a=1&b=2");
}

TEST_CASE("CurlHttpClient 保存 Set-Cookie 并在后续请求发送 Cookie", "[curl][http][cookie]") {
    UBAANext::CookieJar cookies;
    UBAANext::Platform::Curl::CurlHttpClient client(cookies);

    LocalHttpServer first("HTTP/1.1 200 OK\r\nSet-Cookie: SESSION=abc; Domain=127.0.0.1; Path=/\r\nContent-Length: 2\r\n\r\nok");
    UBAANext::HttpRequest first_request;
    first_request.url = first.url("/login");
    first_request.transport.request_timeout_ms = 5000;
    auto first_response = client.send(first_request);
    REQUIRE(first_response.has_value());
    CHECK(cookies.to_header("127.0.0.1") == "SESSION=abc");

    LocalHttpServer second("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
    UBAANext::HttpRequest second_request;
    second_request.url = second.url("/profile");
    second_request.transport.request_timeout_ms = 5000;
    auto second_response = client.send(second_request);
    REQUIRE(second_response.has_value());
    CHECK(header_value(second.request_text(), "Cookie") == "SESSION=abc");
}

TEST_CASE("CurlNetworkStack 的 HTTP client 和 CookieStore 共享 live CookieJar", "[curl][http][cookie]") {
    UBAANext::Platform::Curl::CurlNetworkStack stack;
    auto &client = stack.http_client();

    LocalHttpServer server("HTTP/1.1 200 OK\r\nSet-Cookie: TOKEN=xyz; Domain=127.0.0.1; Path=/\r\nContent-Length: 2\r\n\r\nok");
    UBAANext::HttpRequest request;
    request.url = server.url("/login");
    request.transport.request_timeout_ms = 5000;
    auto response = client.send(request);
    REQUIRE(response.has_value());

    auto &curl_store = static_cast<UBAANext::Platform::Curl::CurlCookieStore &>(stack.cookie_store());
    CHECK(curl_store.live_cookies().to_header("127.0.0.1") == "TOKEN=xyz");
}
