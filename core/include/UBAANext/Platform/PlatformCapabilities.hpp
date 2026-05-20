#pragma once

namespace UBAANext {

struct PlatformCapabilities {
    bool real_network = false;
    bool secure_cookie_persistence = false;
    bool cookie_persistence = false;
    bool redirect_control = false;
    bool openssl_crypto = false;
    bool secure_store = false;
    bool app_data_path = false;
    bool upload_bytes = true;
    bool live_login = false;
    bool write_operations = false;
};

class IPlatformCapabilities {
public:
    virtual ~IPlatformCapabilities() = default;
    [[nodiscard]] virtual PlatformCapabilities capabilities() const = 0;
};

} // namespace UBAANext
