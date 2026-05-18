#pragma once

#include <UBAANext/Crypto/CryptoProvider.hpp>

namespace UBAANext::Platform::OpenSSL {

class OpenSslCryptoProvider final : public UBAANext::ICryptoProvider {
public:
    [[nodiscard]] Result<std::string> md5_hex(const std::string &input) override;
    [[nodiscard]] Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) override;
    [[nodiscard]] Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data,
                                                                     const std::string &key,
                                                                     const std::string &iv) override;
    [[nodiscard]] Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data,
                                                                           const std::string &key) override;
    [[nodiscard]] Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data,
                                                                           const std::string &key) override;
    [[nodiscard]] Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data,
                                                               const std::string &public_key_der_base64) override;
};

} // namespace UBAANext::Platform::OpenSSL
