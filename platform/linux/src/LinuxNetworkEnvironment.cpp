#include <UBAANext/Platform/Linux/LinuxNetworkEnvironment.hpp>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace UBAANext::Platform::Linux {
namespace {

bool connect_with_timeout(const char *host, const char *port, int timeout_ms) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *result = nullptr;
    if (getaddrinfo(host, port, &hints, &result) != 0) return false;
    for (auto *addr = result; addr != nullptr; addr = addr->ai_next) {
        int fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd < 0) continue;
        const int flags = fcntl(fd, F_GETFL, 0);
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        const int rc = connect(fd, addr->ai_addr, addr->ai_addrlen);
        if (rc == 0 || errno == EINPROGRESS) {
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(fd, &write_set);
            timeval tv{};
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(fd + 1, nullptr, &write_set, nullptr, &tv) > 0) {
                int error = 0;
                socklen_t len = sizeof(error);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
                close(fd);
                freeaddrinfo(result);
                return error == 0;
            }
        }
        close(fd);
    }
    freeaddrinfo(result);
    return false;
}

} // namespace

Result<bool> LinuxNetworkEnvironment::is_on_campus_network() {
    return connect_with_timeout("gw.buaa.edu.cn", "80", 500);
}

Result<std::string> LinuxNetworkEnvironment::local_ipv4() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return make_error(ErrorCode::NetworkError, "创建 UDP socket 失败");
    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(80);
    inet_pton(AF_INET, "1.1.1.1", &remote.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr *>(&remote), sizeof(remote)) != 0) {
        close(fd);
        return make_error(ErrorCode::NetworkError, "探测本机 IPv4 失败");
    }
    sockaddr_in local{};
    socklen_t len = sizeof(local);
    if (getsockname(fd, reinterpret_cast<sockaddr *>(&local), &len) != 0) {
        close(fd);
        return make_error(ErrorCode::NetworkError, "读取本机 IPv4 失败");
    }
    char buffer[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &local.sin_addr, buffer, sizeof(buffer));
    close(fd);
    std::string ip = buffer;
    if (ip.empty() || ip == "127.0.0.1") return make_error(ErrorCode::NetworkError, "未获取到非 loopback IPv4");
    return ip;
}

} // namespace UBAANext::Platform::Linux
