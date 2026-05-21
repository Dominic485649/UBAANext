#pragma once

#include <UBAANext/Platform/PlatformCapabilities.hpp>

namespace UBAANext::Platform::Linux {

class LinuxPlatformCapabilities final : public UBAANext::IPlatformCapabilities {
public:
    [[nodiscard]] PlatformCapabilities capabilities() const override;
};

} // namespace UBAANext::Platform::Linux
