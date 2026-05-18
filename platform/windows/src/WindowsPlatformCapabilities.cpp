#include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>

namespace UBAANext::Platform::Windows {

PlatformCapabilities WindowsPlatformCapabilities::capabilities() const {
    PlatformCapabilities caps;
    caps.real_network = true;
    caps.secure_cookie_persistence = false;
    caps.redirect_control = true;
    caps.openssl_crypto = true;
    caps.secure_store = true;
    caps.app_data_path = true;
    caps.upload_bytes = true;
    caps.live_login = true;
    return caps;
}

} // namespace UBAANext::Platform::Windows
