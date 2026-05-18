#pragma once

#include <UBAANext/Platform/PlatformCapabilities.hpp>

namespace UBAANext::Platform::Windows {

class WindowsPlatformCapabilities final : public UBAANext::IPlatformCapabilities {
public:
    [[nodiscard]] PlatformCapabilities capabilities() const override;
};

} // namespace UBAANext::Platform::Windows
