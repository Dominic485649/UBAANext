#include <UBAANext/Platform/Tcp/TdTcpTransport.hpp>
#include <UBAANext/Protocol/TdClient.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

namespace proto = UBAANext::Protocol::Td;
namespace tcp = UBAANext::Platform::Tcp;
namespace um = UBAANext;

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

bool send_bytes(SocketHandle socket, const std::uint8_t *data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int n = ::send(socket, reinterpret_cast<const char *>(data + sent), static_cast<int>(size - sent), 0);
#else
        const ssize_t n = ::send(socket, data + sent, size - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool recv_exact(SocketHandle socket, std::uint8_t *data, std::size_t size) {
    std::size_t received = 0;
    while (received < size) {
#ifdef _WIN32
        const int n = ::recv(socket, reinterpret_cast<char *>(data + received), static_cast<int>(size - received), 0);
#else
        const ssize_t n = ::recv(socket, data + received, size - received, 0);
#endif
        if (n <= 0) return false;
        received += static_cast<std::size_t>(n);
    }
    return true;
}

std::uint32_t read_be_u32(const std::array<std::uint8_t, 5> &header) {
    return (static_cast<std::uint32_t>(header[0]) << 24U) |
           (static_cast<std::uint32_t>(header[1]) << 16U) |
           (static_cast<std::uint32_t>(header[2]) << 8U) |
           static_cast<std::uint32_t>(header[3]);
}

class LocalTdServer {
public:
    explicit LocalTdServer(proto::ByteVector response_frame) : m_response_frame(std::move(response_frame)) {
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

    ~LocalTdServer() {
        wait();
        if (m_server != invalid_socket) close_socket(m_server);
    }

    int port() const { return static_cast<int>(m_port); }

    void wait() {
        if (m_thread.joinable()) m_thread.join();
    }

    const proto::ByteVector &request_frame() const { return m_request_frame; }

private:
    void serve_once() {
        SocketHandle client = ::accept(m_server, nullptr, nullptr);
        if (client == invalid_socket) return;

        std::array<std::uint8_t, 5> header{};
        if (recv_exact(client, header.data(), header.size())) {
            const auto body_length = read_be_u32(header);
            m_request_frame.insert(m_request_frame.end(), header.begin(), header.end());
            m_request_frame.resize(header.size() + body_length);
            if (!recv_exact(client, m_request_frame.data() + header.size(), body_length)) {
                close_socket(client);
                return;
            }
        }

        if (!m_response_frame.empty()) {
            const auto header_size = std::min<std::size_t>(5, m_response_frame.size());
            (void)send_bytes(client, m_response_frame.data(), header_size);
            if (m_response_frame.size() > header_size) {
                (void)send_bytes(client, m_response_frame.data() + header_size, m_response_frame.size() - header_size);
            }
        }
        close_socket(client);
    }

    SocketRuntime m_runtime;
    SocketHandle m_server = invalid_socket;
    unsigned short m_port = 0;
    proto::ByteVector m_response_frame;
    proto::ByteVector m_request_frame;
    std::thread m_thread;
};

proto::ByteVector frame(std::uint8_t request_type, const std::string &body) {
    const proto::ByteVector bytes(body.begin(), body.end());
    auto encoded = proto::encode_frame(request_type, bytes);
    REQUIRE(encoded);
    return encoded.value();
}

} // namespace

TEST_CASE("TD TCP transport 通过本地 loopback 收发完整协议帧", "[Td][Tcp]") {
    const auto response_frame = frame(proto::check_request_type, R"({"status":"success","srvresp":"ok"})");
    LocalTdServer server(response_frame);
    tcp::TdTcpTransport transport;
    proto::TdEndpoint endpoint;
    endpoint.ip = "127.0.0.1";
    endpoint.port = server.port();
    endpoint.timeout_seconds = 2;
    const auto request_frame = frame(proto::check_request_type, R"({"hello":"td"})");

    const auto response = transport.exchange(endpoint, request_frame);

    server.wait();
    REQUIRE(response);
    CHECK(response.value() == response_frame);
    CHECK(server.request_frame() == request_frame);
}

TEST_CASE("TD TCP transport 在本地连接关闭时返回网络错误", "[Td][Tcp]") {
    proto::ByteVector header_only{0, 0, 0, 3, proto::check_request_type};
    LocalTdServer server(header_only);
    tcp::TdTcpTransport transport;
    proto::TdEndpoint endpoint;
    endpoint.ip = "127.0.0.1";
    endpoint.port = server.port();
    endpoint.timeout_seconds = 2;
    const auto request_frame = frame(proto::check_request_type, "{}");

    const auto response = transport.exchange(endpoint, request_frame);

    server.wait();
    REQUIRE_FALSE(response);
    CHECK(response.error().code == um::ErrorCode::NetworkError);
}

TEST_CASE("TD TCP transport 拒绝非法输入和空响应 body", "[Td][Tcp]") {
    tcp::TdTcpTransport transport;
    proto::TdEndpoint endpoint;
    endpoint.ip = "127.0.0.1";
    endpoint.port = 8888;
    endpoint.timeout_seconds = 2;

    auto short_request = transport.exchange(endpoint, {0, 1, 2});
    REQUIRE_FALSE(short_request);
    CHECK(short_request.error().code == um::ErrorCode::InvalidArgument);

    auto request_frame = frame(proto::check_request_type, "{}");
    endpoint.ip.clear();
    auto empty_host = transport.exchange(endpoint, request_frame);
    REQUIRE_FALSE(empty_host);
    CHECK(empty_host.error().code == um::ErrorCode::InvalidArgument);

    proto::ByteVector empty_response{0, 0, 0, 0, proto::check_request_type};
    LocalTdServer server(empty_response);
    endpoint.ip = "127.0.0.1";
    endpoint.port = server.port();
    auto response = transport.exchange(endpoint, request_frame);

    server.wait();
    REQUIRE_FALSE(response);
    CHECK(response.error().code == um::ErrorCode::ParseError);
}
