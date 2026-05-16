#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <UBAANext/Base/Error.hpp>

#include <iomanip>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

namespace UBAANext {
namespace {

#ifdef _WIN32
std::string bytes_to_hex(const std::vector<unsigned char> &data) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : data) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

struct AlgHandle {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~AlgHandle() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};

struct KeyHandle {
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~KeyHandle() { if (handle) BCryptDestroyKey(handle); }
};

class PlatformCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &input) override {
        AlgHandle alg;
        if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_MD5_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 MD5 算法失败");
        DWORD hash_len = 0;
        DWORD cb = 0;
        if (BCryptGetProperty(alg.handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_len), sizeof(hash_len), &cb, 0) != 0) return make_error(ErrorCode::NetworkError, "获取 MD5 长度失败");
        std::vector<unsigned char> hash(hash_len);
        if (BCryptHash(alg.handle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char *>(input.data())), static_cast<ULONG>(input.size()), hash.data(), static_cast<ULONG>(hash.size())) != 0) return make_error(ErrorCode::NetworkError, "计算 MD5 失败");
        return bytes_to_hex(hash);
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        if (key.size() != 16 && key.size() != 24 && key.size() != 32) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        AlgHandle alg;
        if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 AES 算法失败");
        if (BCryptSetProperty(alg.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_CBC)), static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_CBC) + 1) * sizeof(wchar_t)), 0) != 0) return make_error(ErrorCode::NetworkError, "设置 AES CBC 模式失败");
        KeyHandle key_handle;
        std::vector<unsigned char> key_bytes(key.begin(), key.end());
        if (BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0, key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) return make_error(ErrorCode::NetworkError, "生成 AES 密钥失败");

        std::vector<unsigned char> iv_bytes(iv.begin(), iv.end());
        ULONG out_len = 0;
        if (BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_bytes.data(), static_cast<ULONG>(iv_bytes.size()), nullptr, 0, &out_len, 0) != 0) return make_error(ErrorCode::NetworkError, "计算 AES 输出长度失败");
        std::vector<unsigned char> out(out_len);
        iv_bytes.assign(iv.begin(), iv.end());
        if (BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_bytes.data(), static_cast<ULONG>(iv_bytes.size()), out.data(), static_cast<ULONG>(out.size()), &out_len, 0) != 0) return make_error(ErrorCode::NetworkError, "AES 加密失败");
        out.resize(out_len);
        return out;
    }
};
#else
class PlatformCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &) override {
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 MD5 实现");
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        if (key.size() != 16 && key.size() != 24 && key.size() != 32) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES CBC 加密实现");
    }
};
#endif

} // namespace

ICryptoProvider &default_crypto_provider() {
    static PlatformCryptoProvider provider;
    return provider;
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

} // namespace UBAANext
