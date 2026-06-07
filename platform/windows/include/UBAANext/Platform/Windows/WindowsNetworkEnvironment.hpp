#pragma once

#include <UBAANext/Net/NetworkEnvironment.hpp>

namespace UBAANext::Platform::Windows {

class WindowsNetworkEnvironment final : public UBAANext::INetworkEnvironment {
public:
    [[nodiscard]] Result<bool> is_on_campus_network() override;
    [[nodiscard]] Result<std::string> local_ipv4() override;
};

} // namespace UBAANext::Platform::Windows
