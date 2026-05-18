#pragma once

namespace UBAANext::Platform::OpenSSL {

class OpenSslRuntime {
public:
    OpenSslRuntime();
    ~OpenSslRuntime();

    OpenSslRuntime(const OpenSslRuntime &) = delete;
    OpenSslRuntime &operator=(const OpenSslRuntime &) = delete;
};

} // namespace UBAANext::Platform::OpenSSL
