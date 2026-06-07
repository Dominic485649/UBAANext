#pragma once

#include <string>
#include <vector>

namespace UBAANext::Crypto {

[[nodiscard]] std::vector<unsigned char> md5_digest(const std::vector<unsigned char> &data);
[[nodiscard]] std::vector<unsigned char> hmac_md5_digest(const std::vector<unsigned char> &key,
                                                         const std::vector<unsigned char> &message);
[[nodiscard]] std::string bytes_to_hex(const std::vector<unsigned char> &data);
[[nodiscard]] std::string srun_xencode(const std::vector<unsigned char> &plain,
                                       const std::vector<unsigned char> &key);

} // namespace UBAANext::Crypto
