#include <UBAANext/Platform/Windows/WindowsAppDataPathProvider.hpp>

#include <cstdlib>

namespace UBAANext::Platform::Windows {

Result<std::filesystem::path> WindowsAppDataPathProvider::app_data_dir() const {
    const char *local_app_data = std::getenv("LOCALAPPDATA");
    if (!local_app_data || std::string(local_app_data).empty()) {
        return make_error(ErrorCode::NotImplemented, "当前平台未提供 AppData 路径");
    }
    return std::filesystem::path(local_app_data) / "UBAANext";
}

} // namespace UBAANext::Platform::Windows
