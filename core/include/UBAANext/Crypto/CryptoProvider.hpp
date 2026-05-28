/**
 * @file CryptoProvider.hpp
 * @brief Crypto provider boundary for CAS/WebVPN/downstream protocols.
 *
 * UnsupportedCrypto implementations must fail closed; cryptographic helpers may process credentials, tickets, or tokens.
 */
#pragma once

#include <UBAANext/Base/Result.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class ICryptoProvider {
public:
    virtual ~ICryptoProvider() = default;

    /** Sensitive input: hashes protocol inputs; UnsupportedCrypto must return a stable error. */
    [[nodiscard]] virtual Result<std::string> md5_hex(const std::string &input) = 0;
    /** Sensitive input: digests protocol payloads; returned bytes must not be logged verbatim. */
    [[nodiscard]] virtual Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) = 0;
    /** Sensitive input: AES-CBC encrypts protocol data; failures must not fall back to plaintext. */
    [[nodiscard]] virtual Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data,
                                                                             const std::string &key,
                                                                             const std::string &iv) = 0;
    /** Sensitive input: AES-ECB encrypts protocol data; failures must not fall back to plaintext. */
    [[nodiscard]] virtual Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data,
                                                                                   const std::string &key) = 0;
    /** Sensitive input/output: AES-ECB decrypts protocol data that may contain tokens or business payloads. */
    [[nodiscard]] virtual Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data,
                                                                                   const std::string &key) = 0;
    /** Sensitive input: RSA encrypts login/protocol data; UnsupportedCrypto must fail closed. */
    [[nodiscard]] virtual Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data,
                                                                       const std::string &public_key_der_base64) = 0;
};

/** Fallback/Unsupported boundary: returns the configured provider, which may be an unsupported fail-closed implementation. */
[[nodiscard]] ICryptoProvider &default_crypto_provider();
/** Crypto provider injection boundary for tests/platforms; never install plaintext fallback for live protocols. */
void set_default_crypto_provider(ICryptoProvider *provider);
/** Sensitive output: base64 result may encode tokens or encrypted payloads and must not be logged blindly. */
[[nodiscard]] std::string base64_encode(const std::vector<unsigned char> &data);
/** Sensitive input/output: decoded bytes may contain tokens or encrypted payloads. */
[[nodiscard]] std::vector<unsigned char> base64_decode(const std::string &input);

} // namespace UBAANext
