#include <UBAANext/Platform/Linux/LinuxPlatformCapabilities.hpp>

#ifndef UBAANEXT_ENABLE_LINUX_LIBSECRET
#define UBAANEXT_ENABLE_LINUX_LIBSECRET 0
#endif

namespace UBAANext::Platform::Linux {

PlatformCapabilities LinuxPlatformCapabilities::capabilities() const {
    PlatformCapabilities caps;
    caps.real_network = true;
    caps.secure_cookie_persistence = UBAANEXT_ENABLE_LINUX_LIBSECRET;
    caps.cookie_persistence = UBAANEXT_ENABLE_LINUX_LIBSECRET;
    caps.redirect_control = true;
    caps.openssl_crypto = true;
    caps.secure_store = UBAANEXT_ENABLE_LINUX_LIBSECRET;
    caps.app_data_path = true;
    caps.upload_bytes = true;
    caps.live_login = UBAANEXT_ENABLE_LINUX_LIBSECRET;
    caps.write_operations = true;
    return caps;
}

} // namespace UBAANext::Platform::Linux
