#pragma once

#include <UBAANext/Platform/PlatformCapabilities.hpp>

namespace UBAANext::Platform::Harmony {

class HarmonyPlatformCapabilities final : public UBAANext::IPlatformCapabilities {
public:
    [[nodiscard]] PlatformCapabilities capabilities() const override;
};

} // namespace UBAANext::Platform::Harmony
