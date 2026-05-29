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
    /** Placeholder/Upload boundary: true only means byte upload plumbing exists, not that a business upload API is implemented. */
    bool upload_bytes = true;
    /** Live login gate: false means credentials/session flows must fail closed or use mock-only behavior. */
    bool live_login = false;
    /** WriteGated capability: real remote mutations require this plus per-command confirmation. */
    bool write_operations = false;
};

class IPlatformCapabilities {
public:
    virtual ~IPlatformCapabilities() = default;
    /** Platform adapter boundary: exposes capability flags; unsupported/fallback flags must not be interpreted as completion. */
    [[nodiscard]] virtual PlatformCapabilities capabilities() const = 0;
};

} // namespace UBAANext
