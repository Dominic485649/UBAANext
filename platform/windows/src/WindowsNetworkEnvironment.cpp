#include <UBAANext/Platform/Windows/WindowsNetworkEnvironment.hpp>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <string>

namespace UBAANext::Platform::Windows {
namespace {

class WinsockSession {
public:
    WinsockSession() {
        WSADATA data{};
        m_ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    ~WinsockSession() {
        if (m_ok) WSACleanup();
    }
    [[nodiscard]] bool ok() const { return m_ok; }
private:
    bool m_ok = false;
};

bool connect_with_timeout(const char *host, const char *port, int timeout_ms) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *result = nullptr;
    if (getaddrinfo(host, port, &hints, &result) != 0) return false;
    for (auto *addr = result; addr != nullptr; addr = addr->ai_next) {
        SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
        u_long nonblocking = 1;
        ioctlsocket(sock, FIONBIO, &nonblocking);
        const auto rc = connect(sock, addr->ai_addr, static_cast<int>(addr->ai_addrlen));
        if (rc == 0 || WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(sock, &write_set);
            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(0, nullptr, &write_set, nullptr, &tv) > 0) {
                int error = 0;
                int len = sizeof(error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&error), &len);
                closesocket(sock);
                freeaddrinfo(result);
                return error == 0;
            }
        }
        closesocket(sock);
    }
    freeaddrinfo(result);
    return false;
}

} // namespace

Result<bool> WindowsNetworkEnvironment::is_on_campus_network() {
    WinsockSession winsock;
    if (!winsock.ok()) return make_error(ErrorCode::NetworkError, "初始化 WinSock 失败");
    return connect_with_timeout("gw.buaa.edu.cn", "80", 500);
}

Result<std::string> WindowsNetworkEnvironment::local_ipv4() {
    WinsockSession winsock;
    if (!winsock.ok()) return make_error(ErrorCode::NetworkError, "初始化 WinSock 失败");
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return make_error(ErrorCode::NetworkError, "创建 UDP socket 失败");
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "1.1.1.1", &remote.sin_addr);
    if (connect(sock, reinterpret_cast<sockaddr *>(&remote), sizeof(remote)) != 0) {
        closesocket(sock);
        return make_error(ErrorCode::NetworkError, "探测本机 IPv4 失败");
    }
    sockaddr_in local{};
    int len = sizeof(local);
    if (getsockname(sock, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
        closesocket(sock);
        return make_error(ErrorCode::NetworkError, "读取本机 IPv4 失败");
    }
    char buffer[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &local.sin_addr, buffer, sizeof(buffer));
    closesocket(sock);
    std::string ip = buffer;
    if (ip.empty() || ip == "127.0.0.1") return make_error(ErrorCode::NetworkError, "未获取到非 loopback IPv4");
    return ip;
}

} // namespace UBAANext::Platform::Windows
