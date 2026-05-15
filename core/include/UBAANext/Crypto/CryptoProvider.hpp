#pragma once

#include <UBAANext/Base/Result.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class ICryptoProvider {
public:
    virtual ~ICryptoProvider() = default;

    [[nodiscard]] virtual Result<std::string> md5_hex(const std::string &input) = 0;
    [[nodiscard]] virtual Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data,
                                                                             const std::string &key,
                                                                             const std::string &iv) = 0;
};

[[nodiscard]] ICryptoProvider &default_crypto_provider();
[[nodiscard]] std::string base64_encode(const std::vector<unsigned char> &data);

} // namespace UBAANext
