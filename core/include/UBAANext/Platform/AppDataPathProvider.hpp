#pragma once

#include <UBAANext/Base/Result.hpp>

#include <filesystem>

namespace UBAANext {

class IAppDataPathProvider {
public:
    virtual ~IAppDataPathProvider() = default;
    [[nodiscard]] virtual Result<std::filesystem::path> app_data_dir() const = 0;
};

} // namespace UBAANext
