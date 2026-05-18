#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <array>
#include <cctype>

namespace UBAANext {
namespace {

bool valid_aes_key(const std::string &key) {
    return key.size() == 16 || key.size() == 24 || key.size() == 32;
}

class UnsupportedCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &) override {
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 MD5 实现");
    }

    Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &) override {
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 SHA-1 实现");
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data,
                                                       const std::string &key,
                                                       const std::string &iv) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES CBC 加密实现");
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &,
                                                             const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES ECB 加密实现");
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data,
                                                             const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES ECB 密文长度必须为 16 字节倍数");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES ECB 解密实现");
    }

    Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &,
                                                 const std::string &public_key_der_base64) override {
        if (public_key_der_base64.empty()) return make_error(ErrorCode::InvalidArgument, "RSA 公钥不能为空");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 RSA PKCS#1 加密实现");
    }
};

ICryptoProvider *g_default_provider = nullptr;

} // namespace

ICryptoProvider &default_crypto_provider() {
    static UnsupportedCryptoProvider unsupported_provider;
    return g_default_provider ? *g_default_provider : unsupported_provider;
}

void set_default_crypto_provider(ICryptoProvider *provider) {
    g_default_provider = provider;
}

std::string base64_encode(const std::vector<unsigned char> &data) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < data.size(); i += 3) {
        unsigned int value = data[i] << 16;
        if (i + 1 < data.size()) value |= data[i + 1] << 8;
        if (i + 2 < data.size()) value |= data[i + 2];
        out.push_back(alphabet[(value >> 18) & 0x3f]);
        out.push_back(alphabet[(value >> 12) & 0x3f]);
        out.push_back(i + 1 < data.size() ? alphabet[(value >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < data.size() ? alphabet[value & 0x3f] : '=');
    }
    return out;
}

std::vector<unsigned char> base64_decode(const std::string &input) {
    std::array<int, 256> table{};
    table.fill(-1);
    for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i])] = i;
    std::vector<unsigned char> out;
    int val = 0;
    int valb = -8;
    for (unsigned char c : input) {
        if (std::isspace(c) || c == '=') continue;
        if (table[c] == -1) break;
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace UBAANext
