#include <UBAANext/Platform/Linux/LocalEncryptedFileStore.hpp>

#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace UBAANext::Platform::Linux {
namespace {

constexpr const char *kStorePrefix = "UBAANext-LinuxEncryptedStore-v1\n";
constexpr std::size_t kSaltSize = 16;
constexpr std::size_t kIvSize = 16;
constexpr std::size_t kKeySize = 32;

struct EvpCipherCtxDeleter {
    void operator()(EVP_CIPHER_CTX *ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

std::string escape_value(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '\t': escaped += "\\t"; break;
        case '\r': escaped += "\\r"; break;
        case '\n': escaped += "\\n"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string unescape_value(const std::string &value) {
    std::string unescaped;
    unescaped.reserve(value.size());
    bool escaping = false;
    for (char ch : value) {
        if (!escaping) {
            if (ch == '\\') escaping = true;
            else unescaped += ch;
            continue;
        }
        switch (ch) {
        case '\\': unescaped += '\\'; break;
        case 't': unescaped += '\t'; break;
        case 'r': unescaped += '\r'; break;
        case 'n': unescaped += '\n'; break;
        default:
            unescaped += '\\';
            unescaped += ch;
            break;
        }
        escaping = false;
    }
    if (escaping) unescaped += '\\';
    return unescaped;
}

std::string serialize_data(const std::unordered_map<std::string, std::string> &data) {
    std::ostringstream out;
    for (const auto &[key, value] : data) {
        out << escape_value(key) << '\t' << escape_value(value) << '\n';
    }
    return out.str();
}

std::unordered_map<std::string, std::string> parse_data(const std::string &text) {
    std::unordered_map<std::string, std::string> data;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        auto tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) continue;
        std::string key = line.substr(0, tab_pos);
        std::string value = line.substr(tab_pos + 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();
        data[unescape_value(key)] = unescape_value(value);
    }
    return data;
}

std::string environment_string(const char *name) {
    const auto *value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
}

std::string machine_material() {
    std::string material = "UBAANext Linux local encrypted store\n";
    material += "user=" + environment_string("USER") + "\n";
    material += "home=" + environment_string("HOME") + "\n";
#if defined(__linux__)
    std::array<char, 256> host{};
    if (gethostname(host.data(), host.size() - 1) == 0) {
        material += "host=";
        material += host.data();
        material += "\n";
    }
#endif
    return material;
}

std::array<unsigned char, kKeySize> derive_key(const std::vector<unsigned char> &salt) {
    std::array<unsigned char, kKeySize> key{};
    const auto material = machine_material();
    if (PKCS5_PBKDF2_HMAC(material.c_str(), static_cast<int>(material.size()), salt.data(), static_cast<int>(salt.size()), 120000,
                          EVP_sha256(), static_cast<int>(key.size()), key.data()) != 1) {
        throw std::runtime_error("Linux 本地安全存储密钥派生失败");
    }
    return key;
}

std::vector<unsigned char> random_bytes(std::size_t size) {
    std::vector<unsigned char> bytes(size);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("Linux 本地安全存储随机数生成失败");
    }
    return bytes;
}

std::vector<unsigned char> aes_256_cbc_crypt(const std::vector<unsigned char> &input,
                                             const std::array<unsigned char, kKeySize> &key,
                                             const std::vector<unsigned char> &iv,
                                             bool encrypt) {
    std::unique_ptr<EVP_CIPHER_CTX, EvpCipherCtxDeleter> ctx(EVP_CIPHER_CTX_new());
    if (!ctx) throw std::runtime_error("Linux 本地安全存储 AES 上下文创建失败");
    if (EVP_CipherInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, key.data(), iv.data(), encrypt ? 1 : 0) != 1) {
        throw std::runtime_error("Linux 本地安全存储 AES 初始化失败");
    }

    std::vector<unsigned char> output(input.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    int update_len = 0;
    if (EVP_CipherUpdate(ctx.get(), output.data(), &update_len, input.data(), static_cast<int>(input.size())) != 1) {
        throw std::runtime_error(encrypt ? "Linux 本地安全存储加密失败" : "Linux 本地安全存储解密失败");
    }
    int final_len = 0;
    if (EVP_CipherFinal_ex(ctx.get(), output.data() + update_len, &final_len) != 1) {
        throw std::runtime_error(encrypt ? "Linux 本地安全存储加密失败" : "Linux 本地安全存储解密失败");
    }
    output.resize(static_cast<std::size_t>(update_len + final_len));
    return output;
}

} // namespace

LocalEncryptedFileStore::LocalEncryptedFileStore(std::filesystem::path file_path) : m_file_path(std::move(file_path)) {
    load_from_file();
}

LocalEncryptedFileStore::~LocalEncryptedFileStore() {
    (void)save_to_file();
}

void LocalEncryptedFileStore::set_string(const std::string &key, const std::string &value) {
    m_data[key] = value;
    m_dirty = true;
}

std::optional<std::string> LocalEncryptedFileStore::get_string(const std::string &key) const {
    auto it = m_data.find(key);
    if (it == m_data.end()) return std::nullopt;
    return it->second;
}

void LocalEncryptedFileStore::remove(const std::string &key) {
    if (m_data.erase(key) > 0) {
        m_dirty = true;
    }
}

Result<void> LocalEncryptedFileStore::flush() {
    return save_to_file();
}

void LocalEncryptedFileStore::clear() {
    if (!m_data.empty()) {
        m_data.clear();
        m_dirty = true;
    }
}

void LocalEncryptedFileStore::load_from_file() {
    try {
        if (!std::filesystem::exists(m_file_path)) return;
        std::ifstream file(m_file_path, std::ios::binary);
        if (!file.is_open()) return;
        std::string raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (raw.rfind(kStorePrefix, 0) != 0) return;

        std::istringstream lines(raw.substr(std::string(kStorePrefix).size()));
        std::string salt_line;
        std::string iv_line;
        std::string payload_line;
        if (!std::getline(lines, salt_line) || !std::getline(lines, iv_line) || !std::getline(lines, payload_line)) return;
        auto salt = base64_decode(salt_line);
        auto iv = base64_decode(iv_line);
        auto payload = base64_decode(payload_line);
        if (salt.size() != kSaltSize || iv.size() != kIvSize || payload.empty()) return;

        auto key = derive_key(salt);
        auto plain = aes_256_cbc_crypt(payload, key, iv, false);
        m_data = parse_data(std::string(plain.begin(), plain.end()));
        m_dirty = false;
    } catch (...) {
        m_data.clear();
    }
}

Result<void> LocalEncryptedFileStore::save_to_file() const {
    if (!m_dirty) return {};
    try {
        auto parent = m_file_path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        auto salt = random_bytes(kSaltSize);
        auto iv = random_bytes(kIvSize);
        auto key = derive_key(salt);
        const auto plain_text = serialize_data(m_data);
        const std::vector<unsigned char> plain(plain_text.begin(), plain_text.end());
        auto encrypted = aes_256_cbc_crypt(plain, key, iv, true);

        std::ostringstream out;
        out << kStorePrefix;
        out << base64_encode(salt) << '\n';
        out << base64_encode(iv) << '\n';
        out << base64_encode(encrypted) << '\n';

        std::ofstream file(m_file_path, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return make_error(ErrorCode::StorageError, "Linux 本地安全存储文件无法写入");
        const auto raw = out.str();
        file.write(raw.data(), static_cast<std::streamsize>(raw.size()));
        if (!file.good()) return make_error(ErrorCode::StorageError, "Linux 本地安全存储文件写入失败");
        m_dirty = false;
        return {};
    } catch (const std::exception &error) {
        return make_error(ErrorCode::StorageError, error.what());
    }
}

} // namespace UBAANext::Platform::Linux
