#pragma once

#include <UBAANext/Base/Result.hpp>

#include <filesystem>

namespace UBAANext {

class IAppDataPathProvider {
public:
    virtual ~IAppDataPathProvider() = default;
    /** Unsupported/Fallback boundary: returns an app data directory or a platform error; callers must not invent sensitive paths. */
    [[nodiscard]] virtual Result<std::filesystem::path> app_data_dir() const = 0;
};

} // namespace UBAANext
