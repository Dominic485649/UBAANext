#include <UBAANext/Crypto/ProtocolCrypto.hpp>

#include <array>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <iomanip>

namespace UBAANext::Crypto {
namespace {

class Md5 {
public:
    void update(const unsigned char *input, std::size_t len) {
        auto index = static_cast<std::size_t>((m_count >> 3U) & 0x3FU);
        m_count += static_cast<std::uint64_t>(len) << 3U;
        std::size_t i = 0;
        while (i < len) {
            m_buffer[index++] = input[i++];
            if (index == 64) {
                transform();
                index = 0;
            }
        }
    }

    [[nodiscard]] std::vector<unsigned char> finalize() {
        std::array<unsigned char, 8> bits{};
        auto bit_count = m_count;
        for (int i = 0; i < 8; ++i) bits[i] = static_cast<unsigned char>((bit_count >> (8 * i)) & 0xFFU);
        const unsigned char padding = 0x80;
        update(&padding, 1);
        const unsigned char zero = 0;
        while (((m_count >> 3U) & 0x3FU) != 56) update(&zero, 1);
        update(bits.data(), bits.size());
        std::vector<unsigned char> digest(16);
        for (int i = 0; i < 4; ++i) {
            digest[i * 4] = static_cast<unsigned char>(m_state[i] & 0xFFU);
            digest[i * 4 + 1] = static_cast<unsigned char>((m_state[i] >> 8U) & 0xFFU);
            digest[i * 4 + 2] = static_cast<unsigned char>((m_state[i] >> 16U) & 0xFFU);
            digest[i * 4 + 3] = static_cast<unsigned char>((m_state[i] >> 24U) & 0xFFU);
        }
        return digest;
    }

private:
    void transform() {
        static constexpr std::array<std::uint32_t, 64> s = {
            7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
            5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
            4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
            6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21,
        };
        static constexpr std::array<std::uint32_t, 64> k = {
            0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
            0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
            0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
            0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
            0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
            0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
            0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
            0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
        };
        std::array<std::uint32_t, 16> m{};
        for (int i = 0; i < 16; ++i) {
            m[i] = static_cast<std::uint32_t>(m_buffer[i * 4]) |
                   (static_cast<std::uint32_t>(m_buffer[i * 4 + 1]) << 8U) |
                   (static_cast<std::uint32_t>(m_buffer[i * 4 + 2]) << 16U) |
                   (static_cast<std::uint32_t>(m_buffer[i * 4 + 3]) << 24U);
        }
        auto a = m_state[0];
        auto b = m_state[1];
        auto c = m_state[2];
        auto d = m_state[3];
        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t f = 0;
            std::uint32_t g = 0;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            const auto temp = d;
            d = c;
            c = b;
            b = b + ((a + f + k[i] + m[g]) << s[i] | (a + f + k[i] + m[g]) >> (32U - s[i]));
            a = temp;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
    }

    std::array<std::uint32_t, 4> m_state{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U};
    std::uint64_t m_count = 0;
    std::array<unsigned char, 64> m_buffer{};
};

std::vector<std::uint32_t> str_to_words(const std::vector<unsigned char> &bytes) {
    std::vector<std::uint32_t> out;
    out.reserve((bytes.size() + 3) / 4);
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        std::uint32_t value = 0;
        if (i < bytes.size()) value |= bytes[i];
        if (i + 1 < bytes.size()) value |= static_cast<std::uint32_t>(bytes[i + 1]) << 8U;
        if (i + 2 < bytes.size()) value |= static_cast<std::uint32_t>(bytes[i + 2]) << 16U;
        if (i + 3 < bytes.size()) value |= static_cast<std::uint32_t>(bytes[i + 3]) << 24U;
        out.push_back(value);
    }
    return out;
}

std::string custom_base64(const std::vector<unsigned char> &bytes) {
    static constexpr char alphabet[] = "LVoJPiCN2R8G90yg+hmFHuacZ1OWMnrsSTXkYpUq/3dlbfKwv6xztjI7DeBE45QA";
    std::string out;
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const auto b0 = bytes[i];
        const auto b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const auto b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        out.push_back(alphabet[(b0 >> 2U) & 0x3FU]);
        out.push_back(alphabet[((b0 & 0x03U) << 4U) | ((b1 >> 4U) & 0x0FU)]);
        out.push_back(i + 1 < bytes.size() ? alphabet[((b1 & 0x0FU) << 2U) | ((b2 >> 6U) & 0x03U)] : '=');
        out.push_back(i + 2 < bytes.size() ? alphabet[b2 & 0x3FU] : '=');
    }
    return out;
}

} // namespace

std::vector<unsigned char> md5_digest(const std::vector<unsigned char> &data) {
    Md5 md5;
    if (!data.empty()) md5.update(data.data(), data.size());
    return md5.finalize();
}

std::vector<unsigned char> hmac_md5_digest(const std::vector<unsigned char> &key,
                                           const std::vector<unsigned char> &message) {
    std::array<unsigned char, 64> key_block{};
    if (key.size() > key_block.size()) {
        auto hashed = md5_digest(key);
        std::copy(hashed.begin(), hashed.end(), key_block.begin());
    } else {
        std::copy(key.begin(), key.end(), key_block.begin());
    }
    std::array<unsigned char, 64> ipad{};
    std::array<unsigned char, 64> opad{};
    ipad.fill(0x36);
    opad.fill(0x5c);
    for (std::size_t i = 0; i < key_block.size(); ++i) {
        ipad[i] ^= key_block[i];
        opad[i] ^= key_block[i];
    }
    std::vector<unsigned char> inner;
    inner.reserve(ipad.size() + message.size());
    inner.insert(inner.end(), ipad.begin(), ipad.end());
    inner.insert(inner.end(), message.begin(), message.end());
    auto inner_hash = md5_digest(inner);
    std::vector<unsigned char> outer;
    outer.reserve(opad.size() + inner_hash.size());
    outer.insert(outer.end(), opad.begin(), opad.end());
    outer.insert(outer.end(), inner_hash.begin(), inner_hash.end());
    return md5_digest(outer);
}

std::string bytes_to_hex(const std::vector<unsigned char> &data) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : data) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

std::string srun_xencode(const std::vector<unsigned char> &plain,
                         const std::vector<unsigned char> &key) {
    if (plain.empty()) return {};
    auto pw = str_to_words(plain);
    auto pwdkey = str_to_words(key);
    const auto n = static_cast<std::uint32_t>(pw.size());
    pw.push_back(static_cast<std::uint32_t>(plain.size()));
    while (pwdkey.size() < 4) pwdkey.push_back(0);
    std::uint32_t z = static_cast<std::uint32_t>(plain.size());
    const std::uint32_t c = 2654435769U;
    const auto q = static_cast<std::uint32_t>(6 + 52 / (n + 1));
    std::uint32_t d = 0;
    for (std::uint32_t round = 0; round < q; ++round) {
        d += c;
        const auto e = (d >> 2U) & 3U;
        std::uint32_t p = 0;
        while (p < n) {
            const auto y = pw[p + 1];
            const auto m = ((z >> 5U) ^ (y << 2U)) +
                           (((y >> 3U) ^ (z << 4U)) ^ (d ^ y)) +
                           (pwdkey[(p & 3U) ^ e] ^ z);
            pw[p] += m;
            z = pw[p];
            ++p;
        }
        const auto y = pw[0];
        const auto m = ((z >> 5U) ^ (y << 2U)) +
                       (((y >> 3U) ^ (z << 4U)) ^ (d ^ y)) +
                       (pwdkey[(p & 3U) ^ e] ^ z);
        pw[n] += m;
        z = pw[n];
    }
    std::vector<unsigned char> bytes;
    bytes.reserve(pw.size() * 4);
    for (auto word : pw) {
        bytes.push_back(static_cast<unsigned char>(word & 0xFFU));
        bytes.push_back(static_cast<unsigned char>((word >> 8U) & 0xFFU));
        bytes.push_back(static_cast<unsigned char>((word >> 16U) & 0xFFU));
        bytes.push_back(static_cast<unsigned char>((word >> 24U) & 0xFFU));
    }
    return "{SRBX1}" + custom_base64(bytes);
}

} // namespace UBAANext::Crypto
