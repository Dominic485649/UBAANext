#include <UBAANext/Platform/Linux/LinuxAppDataPathProvider.hpp>

#include <cstdlib>

namespace UBAANext::Platform::Linux {
namespace {

std::filesystem::path home_dir() {
    if (const char *home = std::getenv("HOME")) {
        if (*home != '\0') {
            return home;
        }
    }
    return {};
}

} // namespace

Result<std::filesystem::path> LinuxAppDataPathProvider::app_data_dir() const {
    if (const char *xdg_data = std::getenv("XDG_DATA_HOME")) {
        if (*xdg_data != '\0') {
            return std::filesystem::path(xdg_data) / "UBAANext";
        }
    }

    auto home = home_dir();
    if (!home.empty()) {
        return home / ".local" / "share" / "UBAANext";
    }

    return make_error(ErrorCode::StorageError, "Linux AppData 路径不可用：HOME/XDG_DATA_HOME 未设置");
}

} // namespace UBAANext::Platform::Linux
