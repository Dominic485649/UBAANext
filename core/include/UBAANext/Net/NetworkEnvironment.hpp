#pragma once

#include <UBAANext/Base/Result.hpp>

#include <string>

namespace UBAANext {

class INetworkEnvironment {
public:
    virtual ~INetworkEnvironment() = default;

    /** Platform boundary: true only when BUAA gateway is reachable from the current network. */
    [[nodiscard]] virtual Result<bool> is_on_campus_network() = 0;
    /** Platform boundary: returns the local IPv4 used for outbound traffic; never guesses on failure. */
    [[nodiscard]] virtual Result<std::string> local_ipv4() = 0;
};

class FailClosedNetworkEnvironment final : public INetworkEnvironment {
public:
    [[nodiscard]] Result<bool> is_on_campus_network() override {
        return false;
    }

    [[nodiscard]] Result<std::string> local_ipv4() override {
        return make_error(ErrorCode::NetworkError, "当前平台未提供本机 IPv4 探测");
    }
};

} // namespace UBAANext
