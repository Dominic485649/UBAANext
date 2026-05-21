#include <UBAANext/Platform/Harmony/HarmonyAppDataPathProvider.hpp>

#include <cstdlib>

namespace UBAANext::Platform::Harmony {

Result<std::filesystem::path> HarmonyAppDataPathProvider::app_data_dir() const {
    if (const char *path = std::getenv("UBAANEXT_HARMONY_APPDATA")) {
        if (*path != '\0') {
            return std::filesystem::path(path);
        }
    }
    return make_error(ErrorCode::UnsupportedPlatform, "Harmony AppData 路径需要由应用层沙箱上下文注入");
}

} // namespace UBAANext::Platform::Harmony
