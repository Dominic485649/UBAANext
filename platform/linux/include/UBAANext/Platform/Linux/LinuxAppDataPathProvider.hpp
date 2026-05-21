#pragma once

#include <UBAANext/Platform/AppDataPathProvider.hpp>

namespace UBAANext::Platform::Linux {

class LinuxAppDataPathProvider final : public UBAANext::IAppDataPathProvider {
public:
    [[nodiscard]] Result<std::filesystem::path> app_data_dir() const override;
};

} // namespace UBAANext::Platform::Linux
