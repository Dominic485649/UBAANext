#include <UBAANext/Platform/Windows/WindowsAppDataPathProvider.hpp>

#include <cstdlib>

namespace UBAANext::Platform::Windows {

Result<std::filesystem::path> WindowsAppDataPathProvider::app_data_dir() const {
    char *buf = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&buf, &len, "LOCALAPPDATA") == 0 && buf != nullptr) {
        std::filesystem::path path = std::filesystem::path(buf) / "UBAANext";
        free(buf);
        return path;
    }
    return make_error(ErrorCode::NotImplemented, "当前平台未提供 AppData 路径");
}

} // namespace UBAANext::Platform::Windows
