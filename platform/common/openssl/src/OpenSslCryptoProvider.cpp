#include <UBAANext/Platform/OpenSSL/OpenSslCryptoProvider.hpp>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>

namespace UBAANext::Platform::OpenSSL {
namespace {

struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX *ctx) const { EVP_MD_CTX_free(ctx); }
};

struct EvpCipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX *ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

struct RsaDeleter {
    void operator()(RSA *rsa) const { RSA_free(rsa); }
};

std::string bytes_to_hex(const std::vector<unsigned char> &data) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto byte : data) out << std::setw(2) << static_cast<int>(byte);
    return out.str();
}

bool valid_aes_key(const std::string &key) {
    return key.size() == 16 || key.size() == 24 || key.size() == 32;
}

const EVP_CIPHER *aes_cbc_cipher(size_t key_size) {
    switch (key_size) {
    case 16: return EVP_aes_128_cbc();
    case 24: return EVP_aes_192_cbc();
    case 32: return EVP_aes_256_cbc();
    default: return nullptr;
    }
}

const EVP_CIPHER *aes_ecb_cipher(size_t key_size) {
    switch (key_size) {
    case 16: return EVP_aes_128_ecb();
    case 24: return EVP_aes_192_ecb();
    case 32: return EVP_aes_256_ecb();
    default: return nullptr;
    }
}

Result<std::vector<unsigned char>> digest(const EVP_MD *md, const unsigned char *data, size_t size) {
    std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter> ctx(EVP_MD_CTX_new());
    if (!ctx) return make_error(ErrorCode::NetworkError, "创建摘要上下文失败");
    if (EVP_DigestInit_ex(ctx.get(), md, nullptr) != 1) return make_error(ErrorCode::NetworkError, "初始化摘要失败");
    if (EVP_DigestUpdate(ctx.get(), data, size) != 1) return make_error(ErrorCode::NetworkError, "更新摘要失败");
    std::vector<unsigned char> out(EVP_MD_get_size(md));
    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), out.data(), &out_len) != 1) return make_error(ErrorCode::NetworkError, "计算摘要失败");
    out.resize(out_len);
    return out;
}

Result<std::vector<unsigned char>> aes_crypt(const std::vector<unsigned char> &data,
                                             const std::string &key,
                                             const EVP_CIPHER *cipher,
                                             bool encrypt,
                                             bool padding,
                                             const std::string &iv = {}) {
    std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) return make_error(ErrorCode::NetworkError, "创建 AES 上下文失败");
    auto *key_data = reinterpret_cast<const unsigned char *>(key.data());
    auto *iv_data = iv.empty() ? nullptr : reinterpret_cast<const unsigned char *>(iv.data());
    if (EVP_CipherInit_ex(ctx.get(), cipher, nullptr, key_data, iv_data, encrypt ? 1 : 0) != 1) return make_error(ErrorCode::NetworkError, "初始化 AES 失败");
    if (EVP_CIPHER_CTX_set_padding(ctx.get(), padding ? 1 : 0) != 1) return make_error(ErrorCode::NetworkError, "设置 AES padding 失败");

    std::vector<unsigned char> out(data.size() + static_cast<size_t>(EVP_CIPHER_get_block_size(cipher)));
    int update_len = 0;
    if (EVP_CipherUpdate(ctx.get(), out.data(), &update_len, data.data(), static_cast<int>(data.size())) != 1) return make_error(ErrorCode::NetworkError, encrypt ? "AES 加密失败" : "AES 解密失败");
    int final_len = 0;
    if (EVP_CipherFinal_ex(ctx.get(), out.data() + update_len, &final_len) != 1) return make_error(ErrorCode::NetworkError, encrypt ? "AES 加密失败" : "AES 解密失败");
    out.resize(static_cast<size_t>(update_len + final_len));
    return out;
}

} // namespace

Result<std::string> OpenSslCryptoProvider::md5_hex(const std::string &input) {
    auto out = digest(EVP_md5(), reinterpret_cast<const unsigned char *>(input.data()), input.size());
    if (!out) return make_error(out.error().code, out.error().message);
    return bytes_to_hex(*out);
}

Result<std::vector<unsigned char>> OpenSslCryptoProvider::sha1_digest(const std::vector<unsigned char> &data) {
    return digest(EVP_sha1(), data.data(), data.size());
}

Result<std::vector<unsigned char>> OpenSslCryptoProvider::aes_cbc_encrypt(const std::vector<unsigned char> &data,
                                                                          const std::string &key,
                                                                          const std::string &iv) {
    if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
    if (iv.size() != 16) return make_error(ErrorCode::InvalidArgument, "AES CBC IV 长度必须为 16 字节");
    if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES CBC 明文长度必须为 16 字节倍数");
    return aes_crypt(data, key, aes_cbc_cipher(key.size()), true, false, iv);
}

Result<std::vector<unsigned char>> OpenSslCryptoProvider::aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data,
                                                                                const std::string &key) {
    if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
    return aes_crypt(data, key, aes_ecb_cipher(key.size()), true, true);
}

Result<std::vector<unsigned char>> OpenSslCryptoProvider::aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data,
                                                                                const std::string &key) {
    if (!valid_aes_key(key)) return make_error(ErrorCode::InvalidArgument, "AES key 长度必须为 16、24 或 32 字节");
    if (data.empty() || data.size() % 16 != 0) return make_error(ErrorCode::InvalidArgument, "AES ECB 密文长度必须为 16 字节倍数");
    return aes_crypt(data, key, aes_ecb_cipher(key.size()), false, true);
}

Result<std::string> OpenSslCryptoProvider::rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data,
                                                                    const std::string &public_key_der_base64) {
    auto der = base64_decode(public_key_der_base64);
    if (der.empty()) return make_error(ErrorCode::InvalidArgument, "RSA 公钥不能为空");
    const unsigned char *cursor = der.data();
    std::unique_ptr<RSA, RsaDeleter> rsa(d2i_RSA_PUBKEY(nullptr, &cursor, static_cast<long>(der.size())));
    if (!rsa) return make_error(ErrorCode::NetworkError, "解析 RSA 公钥失败");
    auto block_len = RSA_size(rsa.get());
    if (data.size() > static_cast<size_t>(block_len - 11)) return make_error(ErrorCode::InvalidArgument, "RSA 明文过长");
    std::vector<unsigned char> out(static_cast<size_t>(block_len));
    int encrypted_len = RSA_public_encrypt(static_cast<int>(data.size()), data.data(), out.data(), rsa.get(), RSA_PKCS1_PADDING);
    if (encrypted_len <= 0) return make_error(ErrorCode::NetworkError, "RSA 加密失败");
    out.resize(static_cast<size_t>(encrypted_len));
    return base64_encode(out);
}

} // namespace UBAANext::Platform::OpenSSL
