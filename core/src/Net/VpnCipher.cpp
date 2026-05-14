/**
 * @file VpnCipher.cpp
 * @brief BUAA WebVPN URL 加密实现
 *
 * 加密算法: AES-128-CFB (NoPadding)
 * Key: "wrdvpnisthebest!" (16 bytes)
 * IV:  "wrdvpnisthebest!" (16 bytes)
 *
 */

#include <UBAANext/Net/VpnCipher.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace UBAANext {

static const char *VPN_KEY = "wrdvpnisthebest!";
static const char *VPN_GATEWAY = "d.buaa.edu.cn";

static std::string to_hex(const unsigned char *data, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        result += hex_chars[(data[i] >> 4) & 0x0F];
        result += hex_chars[data[i] & 0x0F];
    }
    return result;
}

namespace {

constexpr std::array<std::uint8_t, 256> kSBox = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

constexpr std::array<std::uint8_t, 11> kRcon = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36,
};

std::uint8_t xtime(std::uint8_t value) {
    return static_cast<std::uint8_t>((value << 1U) ^ ((value & 0x80U) ? 0x1bU : 0x00U));
}

std::array<std::uint8_t, 176> expand_key(const unsigned char *key) {
    std::array<std::uint8_t, 176> round_keys{};
    std::copy_n(key, 16, round_keys.begin());

    int bytes_generated = 16;
    int rcon_iteration = 1;
    std::array<std::uint8_t, 4> temp{};

    while (bytes_generated < static_cast<int>(round_keys.size())) {
        for (int i = 0; i < 4; ++i) {
            temp[i] = round_keys[bytes_generated - 4 + i];
        }

        if (bytes_generated % 16 == 0) {
            std::rotate(temp.begin(), temp.begin() + 1, temp.end());
            for (auto &byte : temp) {
                byte = kSBox[byte];
            }
            temp[0] ^= kRcon[rcon_iteration++];
        }

        for (int i = 0; i < 4; ++i) {
            round_keys[bytes_generated] = round_keys[bytes_generated - 16] ^ temp[i];
            ++bytes_generated;
        }
    }

    return round_keys;
}

void add_round_key(std::array<std::uint8_t, 16> &state,
                   const std::array<std::uint8_t, 176> &round_keys,
                   int round) {
    for (int i = 0; i < 16; ++i) {
        state[i] ^= round_keys[round * 16 + i];
    }
}

void sub_bytes(std::array<std::uint8_t, 16> &state) {
    for (auto &byte : state) {
        byte = kSBox[byte];
    }
}

void shift_rows(std::array<std::uint8_t, 16> &state) {
    auto tmp = state;
    state[1] = tmp[5];
    state[5] = tmp[9];
    state[9] = tmp[13];
    state[13] = tmp[1];
    state[2] = tmp[10];
    state[6] = tmp[14];
    state[10] = tmp[2];
    state[14] = tmp[6];
    state[3] = tmp[15];
    state[7] = tmp[3];
    state[11] = tmp[7];
    state[15] = tmp[11];
}

void mix_columns(std::array<std::uint8_t, 16> &state) {
    for (int col = 0; col < 4; ++col) {
        auto *c = &state[col * 4];
        const std::uint8_t a0 = c[0];
        const std::uint8_t a1 = c[1];
        const std::uint8_t a2 = c[2];
        const std::uint8_t a3 = c[3];
        const std::uint8_t t = static_cast<std::uint8_t>(a0 ^ a1 ^ a2 ^ a3);
        const std::uint8_t u = a0;
        c[0] ^= t ^ xtime(static_cast<std::uint8_t>(a0 ^ a1));
        c[1] ^= t ^ xtime(static_cast<std::uint8_t>(a1 ^ a2));
        c[2] ^= t ^ xtime(static_cast<std::uint8_t>(a2 ^ a3));
        c[3] ^= t ^ xtime(static_cast<std::uint8_t>(a3 ^ u));
    }
}

std::array<std::uint8_t, 16> aes_encrypt_block(const std::array<std::uint8_t, 16> &block,
                                                const std::array<std::uint8_t, 176> &round_keys) {
    auto state = block;
    add_round_key(state, round_keys, 0);
    for (int round = 1; round < 10; ++round) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round_keys, round);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, round_keys, 10);
    return state;
}

std::string aes_cfb128_encrypt(std::string plaintext, const unsigned char *key, const unsigned char *iv_in) {
    if (plaintext.empty()) {
        return {};
    }

    const auto remainder = plaintext.size() % 16;
    if (remainder != 0) {
        plaintext.append(16 - remainder, '0');
    }

    const auto round_keys = expand_key(key);
    std::array<std::uint8_t, 16> feedback{};
    std::copy_n(iv_in, 16, feedback.begin());

    std::string encrypted(plaintext.size(), '\0');
    for (std::size_t offset = 0; offset < plaintext.size(); offset += 16) {
        auto stream = aes_encrypt_block(feedback, round_keys);
        for (std::size_t i = 0; i < 16; ++i) {
            auto cipher_byte = static_cast<std::uint8_t>(static_cast<unsigned char>(plaintext[offset + i]) ^ stream[i]);
            encrypted[offset + i] = static_cast<char>(cipher_byte);
            feedback[i] = cipher_byte;
        }
    }

    return encrypted;
}

} // namespace

std::string VpnCipher::to_vpn_url(const std::string &url) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) return url;

    std::string scheme = url.substr(0, scheme_end);
    size_t host_start = scheme_end + 3;
    auto path_start = url.find('/', host_start);

    std::string host;
    std::string rest;
    if (path_start != std::string::npos) {
        host = url.substr(host_start, path_start - host_start);
        rest = url.substr(path_start);
    } else {
        host = url.substr(host_start);
        rest = "/";
    }

    if (host == VPN_GATEWAY) {
        return url;
    }

    // 处理端口
    std::string host_only = host;
    std::string port_suffix;
    auto colon = host.find(':');
    if (colon != std::string::npos) {
        host_only = host.substr(0, colon);
        std::string port_str = host.substr(colon + 1);
        if (scheme == "https" && port_str == "443") {
            // 默认端口
        } else if (scheme == "http" && port_str == "80") {
            // 默认端口
        } else {
            port_suffix = "-" + port_str;
        }
    }

    std::string enc_host = encrypt_host(host_only);
    std::string proto = scheme;
    if (!port_suffix.empty()) {
        proto += port_suffix;
    }

    return "https://" + std::string(VPN_GATEWAY) + "/" + proto + "/" + enc_host + rest;
}

std::string VpnCipher::encrypt_host(const std::string &host) {
    unsigned char key[16], iv[16];
    memcpy(key, VPN_KEY, 16);
    memcpy(iv, VPN_KEY, 16);

    auto encrypted = aes_cfb128_encrypt(host, key, iv);
    if (encrypted.empty()) {
        return host;  // 降级
    }

    // 输出格式: IV前16字节的hex + 密文hex（截取到 host.size()）
    // 参考 Kotlin 实现的输出格式
    std::string result;

    // 前 16 字节是 IV 的 hex
    result += to_hex(iv, 16);

    // 后面是密文的 hex，截取到 host 长度
    size_t cipher_len = (std::min)(encrypted.size(), host.size());
    result += to_hex(reinterpret_cast<const unsigned char *>(encrypted.data()), cipher_len);

    return result;
}

bool VpnCipher::can_direct_connect() {
    return false;  // 默认使用 VPN
}

} // namespace UBAANext
