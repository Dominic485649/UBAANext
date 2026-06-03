#include <UBAANext/Platform/Tcp/TdTcpTransport.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace UBAANext::Platform::Tcp {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalid_socket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle invalid_socket = -1;
#endif

constexpr std::size_t td_header_size = 5;
constexpr std::uint32_t max_td_response_body_size = 32U * 1024U * 1024U;

class SocketRuntime {
public:
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data{};
        m_ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
        m_ok = true;
#endif
    }

    ~SocketRuntime() {
#ifdef _WIN32
        if (m_ok) WSACleanup();
#endif
    }

    [[nodiscard]] bool ok() const noexcept { return m_ok; }

private:
    bool m_ok = false;
};

class SocketGuard {
public:
    explicit SocketGuard(SocketHandle socket = invalid_socket) : m_socket(socket) {}
    ~SocketGuard() { close(); }

    SocketGuard(const SocketGuard &) = delete;
    SocketGuard &operator=(const SocketGuard &) = delete;

    SocketGuard(SocketGuard &&other) noexcept : m_socket(std::exchange(other.m_socket, invalid_socket)) {}

    SocketGuard &operator=(SocketGuard &&other) noexcept {
        if (this != &other) {
            close();
            m_socket = std::exchange(other.m_socket, invalid_socket);
        }
        return *this;
    }

    [[nodiscard]] SocketHandle get() const noexcept { return m_socket; }
    [[nodiscard]] bool valid() const noexcept { return m_socket != invalid_socket; }

private:
    void close() noexcept {
        if (m_socket == invalid_socket) return;
#ifdef _WIN32
        closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = invalid_socket;
    }

    SocketHandle m_socket = invalid_socket;
};

ErrorCode current_socket_error_code() {
#ifdef _WIN32
    const auto error = WSAGetLastError();
    if (error == WSAETIMEDOUT) return ErrorCode::Timeout;
    return ErrorCode::NetworkError;
#else
    if (errno == ETIMEDOUT || errno == EAGAIN || errno == EWOULDBLOCK) return ErrorCode::Timeout;
    return ErrorCode::NetworkError;
#endif
}

std::string current_socket_error_message() {
#ifdef _WIN32
    return "socket error " + std::to_string(WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

Result<void> set_timeout(SocketHandle socket, int timeout_seconds) {
    if (timeout_seconds <= 0) return make_error(ErrorCode::InvalidArgument, "TD TCP timeout 必须大于 0");
#ifdef _WIN32
    const DWORD timeout_ms = static_cast<DWORD>(timeout_seconds) * 1000U;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout_ms), sizeof(timeout_ms)) != 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout_ms), sizeof(timeout_ms)) != 0) {
        return make_error(current_socket_error_code(), "设置 TD TCP 超时失败: " + current_socket_error_message());
    }
#else
    timeval timeout{};
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        return make_error(current_socket_error_code(), "设置 TD TCP 超时失败: " + current_socket_error_message());
    }
#endif
    return {};
}

std::uint32_t read_be_u32(const std::array<std::uint8_t, td_header_size> &header) {
    return (static_cast<std::uint32_t>(header[0]) << 24U) |
           (static_cast<std::uint32_t>(header[1]) << 16U) |
           (static_cast<std::uint32_t>(header[2]) << 8U) |
           static_cast<std::uint32_t>(header[3]);
}

std::string endpoint_label(const Protocol::Td::TdEndpoint &endpoint) {
    return endpoint.ip + ":" + std::to_string(endpoint.port);
}

Result<SocketGuard> connect_socket(const Protocol::Td::TdEndpoint &endpoint) {
    if (endpoint.ip.empty()) return make_error(ErrorCode::InvalidArgument, "TD TCP endpoint.ip 不能为空");
    if (endpoint.port <= 0 || endpoint.port > 65535) return make_error(ErrorCode::InvalidArgument, "TD TCP endpoint.port 必须在 1..65535 之间");

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo *resolved = nullptr;
    const auto port_text = std::to_string(endpoint.port);
    const auto gai = getaddrinfo(endpoint.ip.c_str(), port_text.c_str(), &hints, &resolved);
    if (gai != 0) {
        return make_error(ErrorCode::NetworkError, "解析 TD TCP endpoint 失败: " + endpoint_label(endpoint));
    }

    struct AddrinfoGuard {
        explicit AddrinfoGuard(addrinfo *value) : ptr(value) {}
        ~AddrinfoGuard() { if (ptr) freeaddrinfo(ptr); }
        addrinfo *ptr = nullptr;
    } guard(resolved);

    ErrorCode last_code = ErrorCode::NetworkError;
    std::string last_message = "无法连接 TD TCP endpoint";
    for (auto *entry = resolved; entry != nullptr; entry = entry->ai_next) {
        SocketGuard socket(::socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol));
        if (!socket.valid()) {
            last_code = current_socket_error_code();
            last_message = current_socket_error_message();
            continue;
        }

        if (auto timeout = set_timeout(socket.get(), endpoint.timeout_seconds); !timeout) {
            return make_error(timeout.error().code, timeout.error().message);
        }

        if (::connect(socket.get(), entry->ai_addr, static_cast<int>(entry->ai_addrlen)) == 0) {
            return socket;
        }
        last_code = current_socket_error_code();
        last_message = current_socket_error_message();
    }

    return make_error(last_code, "连接 TD TCP endpoint 失败: " + endpoint_label(endpoint) + " (" + last_message + ")");
}

Result<void> send_all(SocketHandle socket, const Protocol::Td::ByteVector &bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto remaining = bytes.size() - sent;
#ifdef _WIN32
        const auto chunk_size = remaining > static_cast<std::size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : static_cast<int>(remaining);
        const int n = ::send(socket, reinterpret_cast<const char *>(bytes.data() + sent), chunk_size, 0);
#else
        const ssize_t n = ::send(socket, bytes.data() + sent, remaining, 0);
#endif
        if (n <= 0) {
            return make_error(current_socket_error_code(), "发送 TD TCP 请求失败: " + current_socket_error_message());
        }
        sent += static_cast<std::size_t>(n);
    }
    return {};
}

Result<void> recv_exact(SocketHandle socket, std::uint8_t *buffer, std::size_t size, const std::string &label) {
    std::size_t received = 0;
    while (received < size) {
        const auto remaining = size - received;
#ifdef _WIN32
        const auto chunk_size = remaining > static_cast<std::size_t>(std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max() : static_cast<int>(remaining);
        const int n = ::recv(socket, reinterpret_cast<char *>(buffer + received), chunk_size, 0);
#else
        const ssize_t n = ::recv(socket, buffer + received, remaining, 0);
#endif
        if (n == 0) return make_error(ErrorCode::NetworkError, "TD TCP 连接在读取 " + label + " 时关闭");
        if (n < 0) return make_error(current_socket_error_code(), "读取 TD TCP " + label + " 失败: " + current_socket_error_message());
        received += static_cast<std::size_t>(n);
    }
    return {};
}

} // namespace

Result<Protocol::Td::ByteVector> TdTcpTransport::exchange(const Protocol::Td::TdEndpoint &endpoint,
                                                          const Protocol::Td::ByteVector &request_frame) {
    if (request_frame.size() < td_header_size) return make_error(ErrorCode::InvalidArgument, "TD TCP 请求帧过短");

    SocketRuntime runtime;
    if (!runtime.ok()) return make_error(ErrorCode::UnsupportedNetwork, "TD TCP socket runtime 初始化失败");

    auto socket = connect_socket(endpoint);
    if (!socket) return make_error(socket.error().code, socket.error().message);

    auto sent = send_all(socket->get(), request_frame);
    if (!sent) return make_error(sent.error().code, sent.error().message);

    std::array<std::uint8_t, td_header_size> header{};
    auto header_read = recv_exact(socket->get(), header.data(), header.size(), "header");
    if (!header_read) return make_error(header_read.error().code, header_read.error().message);

    const auto body_length = read_be_u32(header);
    if (body_length == 0) return make_error(ErrorCode::ParseError, "TD TCP 响应 body 为空");
    if (body_length > max_td_response_body_size) {
        return make_error(ErrorCode::ParseError, "TD TCP 响应 body 过大: " + std::to_string(body_length));
    }

    Protocol::Td::ByteVector response_frame;
    response_frame.reserve(td_header_size + body_length);
    response_frame.insert(response_frame.end(), header.begin(), header.end());
    response_frame.resize(td_header_size + body_length);

    auto body_read = recv_exact(socket->get(), response_frame.data() + td_header_size, body_length, "body");
    if (!body_read) return make_error(body_read.error().code, body_read.error().message);

    return response_frame;
}

} // namespace UBAANext::Platform::Tcp
