#pragma once

#include <UBAANext/Platform/AppDataPathProvider.hpp>

namespace UBAANext::Platform::Windows {

class WindowsAppDataPathProvider final : public UBAANext::IAppDataPathProvider {
public:
    [[nodiscard]] Result<std::filesystem::path> app_data_dir() const override;
};

} // namespace UBAANext::Platform::Windows
