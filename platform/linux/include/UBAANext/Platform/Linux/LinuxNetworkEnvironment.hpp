#pragma once

#include <UBAANext/Net/NetworkEnvironment.hpp>

namespace UBAANext::Platform::Linux {

class LinuxNetworkEnvironment final : public UBAANext::INetworkEnvironment {
public:
    [[nodiscard]] Result<bool> is_on_campus_network() override;
    [[nodiscard]] Result<std::string> local_ipv4() override;
};

} // namespace UBAANext::Platform::Linux
