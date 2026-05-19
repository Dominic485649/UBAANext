#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>

#include <catch2/catch_session.hpp>

int main(int argc, char *argv[]) {
    UBAANext::Platform::OpenSSL::install_open_ssl_crypto_provider();
    return Catch::Session().run(argc, argv);
}
