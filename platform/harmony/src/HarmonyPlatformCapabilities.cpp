#include <UBAANext/Platform/Harmony/HarmonyPlatformCapabilities.hpp>

namespace UBAANext::Platform::Harmony {

PlatformCapabilities HarmonyPlatformCapabilities::capabilities() const {
    PlatformCapabilities caps;
    caps.real_network = true;
    caps.secure_cookie_persistence = false;
    caps.cookie_persistence = false;
    caps.redirect_control = true;
    caps.openssl_crypto = true;
    caps.secure_store = false;
    caps.app_data_path = true;
    caps.upload_bytes = true;
    caps.live_login = false;
    caps.write_operations = false;
    return caps;
}

} // namespace UBAANext::Platform::Harmony
