#include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>

#ifndef UBAANEXT_BUILD_DESKTOP
#define UBAANEXT_BUILD_DESKTOP 0
#endif
#ifndef UBAANEXT_ENABLE_WINFSP
#define UBAANEXT_ENABLE_WINFSP 0
#endif
#ifndef UBAANEXT_ENABLE_CLOUD_FILES
#define UBAANEXT_ENABLE_CLOUD_FILES 0
#endif

namespace UBAANext::Platform::Windows {

PlatformCapabilities WindowsPlatformCapabilities::capabilities() const {
    PlatformCapabilities caps;
    caps.real_network = true;
    caps.secure_cookie_persistence = true;
    caps.cookie_persistence = true;
    caps.redirect_control = true;
    caps.protocol_crypto = true;
    caps.secure_store = true;
    caps.app_data_path = true;
    caps.upload_bytes = true;
    caps.live_login = true;
    caps.write_operations = true;
    caps.desktop_gui = UBAANEXT_BUILD_DESKTOP;
    caps.mount_windows_drive = UBAANEXT_ENABLE_WINFSP;
    caps.mount_windows_sync = UBAANEXT_ENABLE_CLOUD_FILES;
    caps.mount_linux_userspace = false;
    return caps;
}

} // namespace UBAANext::Platform::Windows
