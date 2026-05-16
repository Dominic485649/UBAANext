#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <UBAANext/Base/Error.hpp>

#include <array>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

#if defined(UBAANEXT_USE_OH_CRYPTO) && UBAANEXT_USE_OH_CRYPTO
#include <CryptoArchitectureKit/crypto_architecture_kit.h>
#endif

namespace UBAANext {
namespace {

std::string bytes_to_hex(const std::vector<unsigned char> &data) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : data) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

bool valid_aes_key(const std::string &key) {
    return key.size() == 16 || key.size() == 24 || key.size() == 32;
}

std::vector<unsigned char> string_bytes(const std::string &text) {
    return {text.begin(), text.end()};
}

#ifdef _WIN32

struct AlgHandle {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~AlgHandle() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};

struct KeyHandle {
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~KeyHandle() { if (handle) BCryptDestroyKey(handle); }
};

struct CryptoKeyHandle {
    HCRYPTKEY handle = 0;
    ~CryptoKeyHandle() { if (handle) CryptDestroyKey(handle); }
};

struct CryptoProviderHandle {
    HCRYPTPROV handle = 0;
    ~CryptoProviderHandle() { if (handle) CryptReleaseContext(handle, 0); }
};

Result<std::vector<unsigned char>> bcrypt_digest(const wchar_t *algorithm, const std::vector<unsigned char> &data, const std::string &label) {
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.handle, algorithm, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 " + label + " 算法失败");
    DWORD hash_len = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg.handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_len), sizeof(hash_len), &cb, 0) != 0) return make_error(ErrorCode::NetworkError, "获取 " + label + " 长度失败");
    std::vector<unsigned char> hash(hash_len);
    if (BCryptHash(alg.handle, nullptr, 0, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), hash.data(), static_cast<ULONG>(hash.size())) != 0) return make_error(ErrorCode::NetworkError, "计算 " + label + " 失败");
    return hash;
}

Result<std::vector<unsigned char>> bcrypt_aes_crypt(const std::vector<unsigned char> &data,
                                                    const std::string &key,
                                                    const wchar_t *mode,
                                                    ULONG flags,
                                                    bool encrypt,
                                                    const std::string &iv = {}) {
    AlgHandle alg;
    if (BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return make_error(ErrorCode::NetworkError, "打开 AES 算法失败");
    if (BCryptSetProperty(alg.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(mode)), static_cast<ULONG>((wcslen(mode) + 1) * sizeof(wchar_t)), 0) != 0) return make_error(ErrorCode::NetworkError, "设置 AES 模式失败");
    KeyHandle key_handle;
    auto key_bytes = string_bytes(key);
    if (BCryptGenerateSymmetricKey(alg.handle, &key_handle.handle, nullptr, 0, key_bytes.data(), static_cast<ULONG>(key_bytes.size()), 0) != 0) return make_error(ErrorCode::NetworkError, "生成 AES 密钥失败");

    std::vector<unsigned char> iv_bytes(iv.begin(), iv.end());
    PUCHAR iv_data = iv_bytes.empty() ? nullptr : iv_bytes.data();
    auto iv_len = static_cast<ULONG>(iv_bytes.size());
    ULONG out_len = 0;
    NTSTATUS length_status = encrypt
        ? BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_data, iv_len, nullptr, 0, &out_len, flags)
        : BCryptDecrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_data, iv_len, nullptr, 0, &out_len, flags);
    if (length_status != 0) return make_error(ErrorCode::NetworkError, encrypt ? "计算 AES 输出长度失败" : "计算 AES 解密输出长度失败");

    std::vector<unsigned char> out(out_len);
    iv_bytes.assign(iv.begin(), iv.end());
    iv_data = iv_bytes.empty() ? nullptr : iv_bytes.data();
    NTSTATUS crypt_status = encrypt
        ? BCryptEncrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_data, iv_len, out.data(), static_cast<ULONG>(out.size()), &out_len, flags)
        : BCryptDecrypt(key_handle.handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, iv_data, iv_len, out.data(), static_cast<ULONG>(out.size()), &out_len, flags);
    if (crypt_status != 0) return make_error(ErrorCode::NetworkError, encrypt ? "AES 加密失败" : "AES 解密失败");
    out.resize(out_len);
    return out;
}

class PlatformCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &input) override {
        auto digest = bcrypt_digest(BCRYPT_MD5_ALGORITHM, string_bytes(input), "MD5");
        if (!digest) return make_error(digest.error().code, digest.error().message);
        return bytes_to_hex(*digest);
    }

    Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) override {
        return bcrypt_digest(BCRYPT_SHA1_ALGORITHM, data, "SHA-1");
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        return bcrypt_aes_crypt(data, key, BCRYPT_CHAIN_MODE_CBC, 0, true, iv);
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        return bcrypt_aes_crypt(data, key, BCRYPT_CHAIN_MODE_ECB, BCRYPT_BLOCK_PADDING, true);
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES ECB 密文长度必须为 16 字节倍数");
        return bcrypt_aes_crypt(data, key, BCRYPT_CHAIN_MODE_ECB, BCRYPT_BLOCK_PADDING, false);
    }

    Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data, const std::string &public_key_der_base64) override {
        auto der = base64_decode(public_key_der_base64);
        if (der.empty()) return make_error(ErrorCode::InvalidArgument, "RSA 公钥不能为空");
        CERT_PUBLIC_KEY_INFO *info = nullptr;
        DWORD info_len = 0;
        if (!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, X509_PUBLIC_KEY_INFO, der.data(), static_cast<DWORD>(der.size()), CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &info_len)) return make_error(ErrorCode::NetworkError, "解析 RSA 公钥失败");
        std::unique_ptr<CERT_PUBLIC_KEY_INFO, decltype(&LocalFree)> info_guard(info, LocalFree);

        CryptoProviderHandle provider;
        if (!CryptAcquireContextW(&provider.handle, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return make_error(ErrorCode::NetworkError, "初始化 CryptoAPI 失败");
        CryptoKeyHandle key;
        if (!CryptImportPublicKeyInfo(provider.handle, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, info, &key.handle)) return make_error(ErrorCode::NetworkError, "导入 RSA 公钥失败");

        DWORD key_len_bits = 0;
        DWORD key_len_size = sizeof(key_len_bits);
        if (!CryptGetKeyParam(key.handle, KP_KEYLEN, reinterpret_cast<BYTE *>(&key_len_bits), &key_len_size, 0)) return make_error(ErrorCode::NetworkError, "获取 RSA 密钥长度失败");
        DWORD block_len = key_len_bits / 8;
        if (data.size() > block_len - 11) return make_error(ErrorCode::InvalidArgument, "RSA 明文过长");
        std::vector<unsigned char> buffer(block_len);
        std::copy(data.begin(), data.end(), buffer.begin());
        DWORD data_len = static_cast<DWORD>(data.size());
        if (!CryptEncrypt(key.handle, 0, TRUE, 0, buffer.data(), &data_len, static_cast<DWORD>(buffer.size()))) return make_error(ErrorCode::NetworkError, "RSA 加密失败");
        buffer.resize(data_len);
        std::reverse(buffer.begin(), buffer.end());
        return base64_encode(buffer);
    }
};

#elif defined(UBAANEXT_USE_OH_CRYPTO) && UBAANEXT_USE_OH_CRYPTO

struct DigestHandle {
    OH_CryptoDigest *handle = nullptr;
    ~DigestHandle() { if (handle) OH_DigestCrypto_Destroy(handle); }
};

struct SymKeyGeneratorHandle {
    OH_CryptoSymKeyGenerator *handle = nullptr;
    ~SymKeyGeneratorHandle() { if (handle) OH_CryptoSymKeyGenerator_Destroy(handle); }
};

struct SymKeyHandle {
    OH_CryptoSymKey *handle = nullptr;
    SymKeyHandle() = default;
    SymKeyHandle(const SymKeyHandle &) = delete;
    SymKeyHandle &operator=(const SymKeyHandle &) = delete;
    SymKeyHandle(SymKeyHandle &&other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
    SymKeyHandle &operator=(SymKeyHandle &&other) noexcept {
        if (this != &other) {
            if (handle) OH_CryptoSymKey_Destroy(handle);
            handle = std::exchange(other.handle, nullptr);
        }
        return *this;
    }
    ~SymKeyHandle() { if (handle) OH_CryptoSymKey_Destroy(handle); }
};

struct SymCipherHandle {
    OH_CryptoSymCipher *handle = nullptr;
    ~SymCipherHandle() { if (handle) OH_CryptoSymCipher_Destroy(handle); }
};

struct SymCipherParamsHandle {
    OH_CryptoSymCipherParams *handle = nullptr;
    ~SymCipherParamsHandle() { if (handle) OH_CryptoSymCipherParams_Destroy(handle); }
};

struct AsymKeyGeneratorHandle {
    OH_CryptoAsymKeyGenerator *handle = nullptr;
    ~AsymKeyGeneratorHandle() { if (handle) OH_CryptoAsymKeyGenerator_Destroy(handle); }
};

struct KeyPairHandle {
    OH_CryptoKeyPair *handle = nullptr;
    ~KeyPairHandle() { if (handle) OH_CryptoKeyPair_Destroy(handle); }
};

struct AsymCipherHandle {
    OH_CryptoAsymCipher *handle = nullptr;
    ~AsymCipherHandle() { if (handle) OH_CryptoAsymCipher_Destroy(handle); }
};

struct DataBlobHandle {
    Crypto_DataBlob blob{};
    ~DataBlobHandle() { if (blob.data) OH_Crypto_FreeDataBlob(&blob); }
};

Crypto_DataBlob blob_from_bytes(const unsigned char *data, size_t size) {
    Crypto_DataBlob blob{};
    blob.data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(data));
    blob.len = size;
    return blob;
}

std::string aes_key_name(const std::string &key) {
    return "AES" + std::to_string(key.size() * 8);
}

std::string aes_cipher_name(const std::string &key, const std::string &mode, const std::string &padding) {
    return aes_key_name(key) + "|" + mode + "|" + padding;
}

Result<std::vector<unsigned char>> oh_digest(const char *algorithm, const std::vector<unsigned char> &data, const std::string &label) {
    DigestHandle digest;
    if (OH_CryptoDigest_Create(algorithm, &digest.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 " + label + " 摘要失败");
    auto input = blob_from_bytes(data.data(), data.size());
    if (OH_CryptoDigest_Update(digest.handle, &input) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "更新 " + label + " 摘要失败");
    DataBlobHandle out;
    if (OH_CryptoDigest_Final(digest.handle, &out.blob) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "计算 " + label + " 摘要失败");
    return std::vector<unsigned char>(out.blob.data, out.blob.data + out.blob.len);
}

Result<SymKeyHandle> oh_convert_aes_key(const std::string &key) {
    SymKeyGeneratorHandle generator;
    auto name = aes_key_name(key);
    if (OH_CryptoSymKeyGenerator_Create(name.c_str(), &generator.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 AES 密钥生成器失败");
    auto key_bytes = string_bytes(key);
    auto key_blob = blob_from_bytes(key_bytes.data(), key_bytes.size());
    SymKeyHandle sym_key;
    if (OH_CryptoSymKeyGenerator_Convert(generator.handle, &key_blob, &sym_key.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "转换 AES 密钥失败");
    return sym_key;
}

Result<std::vector<unsigned char>> oh_aes_crypt(const std::vector<unsigned char> &data,
                                                const std::string &key,
                                                const std::string &mode,
                                                const std::string &padding,
                                                Crypto_CipherMode cipher_mode,
                                                const std::string &iv = {}) {
    auto sym_key = oh_convert_aes_key(key);
    if (!sym_key) return make_error(sym_key.error().code, sym_key.error().message);

    SymCipherParamsHandle params;
    if (OH_CryptoSymCipherParams_Create(&params.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 AES 参数失败");
    std::vector<unsigned char> iv_bytes(iv.begin(), iv.end());
    if (!iv.empty()) {
        auto iv_blob = blob_from_bytes(iv_bytes.data(), iv_bytes.size());
        if (OH_CryptoSymCipherParams_SetParam(params.handle, CRYPTO_IV_DATABLOB, &iv_blob) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "设置 AES IV 失败");
    }

    SymCipherHandle cipher;
    auto cipher_name = aes_cipher_name(key, mode, padding);
    if (OH_CryptoSymCipher_Create(cipher_name.c_str(), &cipher.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 AES Cipher 失败");
    if (OH_CryptoSymCipher_Init(cipher.handle, cipher_mode, sym_key->handle, params.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "初始化 AES Cipher 失败");

    auto input = blob_from_bytes(data.data(), data.size());
    DataBlobHandle out;
    if (OH_CryptoSymCipher_Final(cipher.handle, &input, &out.blob) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, cipher_mode == CRYPTO_ENCRYPT_MODE ? "AES 加密失败" : "AES 解密失败");
    return std::vector<unsigned char>(out.blob.data, out.blob.data + out.blob.len);
}

class PlatformCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &input) override {
        auto digest = oh_digest("MD5", string_bytes(input), "MD5");
        if (!digest) return make_error(digest.error().code, digest.error().message);
        return bytes_to_hex(*digest);
    }

    Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) override {
        return oh_digest("SHA1", data, "SHA-1");
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        return oh_aes_crypt(data, key, "CBC", "NoPadding", CRYPTO_ENCRYPT_MODE, iv);
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        return oh_aes_crypt(data, key, "ECB", "PKCS7", CRYPTO_ENCRYPT_MODE);
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES ECB 密文长度必须为 16 字节倍数");
        return oh_aes_crypt(data, key, "ECB", "PKCS7", CRYPTO_DECRYPT_MODE);
    }

    Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data, const std::string &public_key_der_base64) override {
        auto der = base64_decode(public_key_der_base64);
        if (der.empty()) return make_error(ErrorCode::InvalidArgument, "RSA 公钥不能为空");
        AsymKeyGeneratorHandle generator;
        if (OH_CryptoAsymKeyGenerator_Create("RSA1024", &generator.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 RSA 密钥生成器失败");
        auto der_blob = blob_from_bytes(der.data(), der.size());
        KeyPairHandle key_pair;
        if (OH_CryptoAsymKeyGenerator_Convert(generator.handle, CRYPTO_DER, &der_blob, nullptr, &key_pair.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "导入 RSA 公钥失败");
        AsymCipherHandle cipher;
        if (OH_CryptoAsymCipher_Create("RSA|PKCS1", &cipher.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "创建 RSA Cipher 失败");
        if (OH_CryptoAsymCipher_Init(cipher.handle, CRYPTO_ENCRYPT_MODE, key_pair.handle) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "初始化 RSA Cipher 失败");
        auto input = blob_from_bytes(data.data(), data.size());
        DataBlobHandle out;
        if (OH_CryptoAsymCipher_Final(cipher.handle, &input, &out.blob) != CRYPTO_SUCCESS) return make_error(ErrorCode::NetworkError, "RSA 加密失败");
        return base64_encode(std::vector<unsigned char>(out.blob.data, out.blob.data + out.blob.len));
    }
};

#else
class PlatformCryptoProvider final : public ICryptoProvider {
public:
    Result<std::string> md5_hex(const std::string &) override {
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 MD5 实现");
    }

    Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &) override {
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 SHA-1 实现");
    }

    Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES CBC 加密实现");
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES ECB 加密实现");
    }

    Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
        if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES ECB 密文长度必须为 16 字节倍数");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 AES ECB 解密实现");
    }

    Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &, const std::string &public_key_der_base64) override {
        if (public_key_der_base64.empty()) return make_error(ErrorCode::InvalidArgument, "RSA 公钥不能为空");
        return make_error(ErrorCode::NotImplemented, "当前平台尚未接入 RSA PKCS#1 加密实现");
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
