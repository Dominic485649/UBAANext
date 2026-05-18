#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>

#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Platform/OpenSSL/OpenSslCryptoProvider.hpp>

namespace UBAANext::Platform::OpenSSL {

void install_open_ssl_crypto_provider() {
    static OpenSslCryptoProvider provider;
    UBAANext::set_default_crypto_provider(&provider);
}

} // namespace UBAANext::Platform::OpenSSL
