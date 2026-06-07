#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace UBAANext::Platform::Linux {

/**
 * Linux local encrypted file store for CLI credentials/session state.
 *
 * This implementation encrypts the whole key-value payload with OpenSSL AES-256-CBC.
 * The key is derived from local machine/user material plus a per-file random salt.
 * It is intended as a built-in fallback when Secret Service is unavailable; it is
 * stronger than plaintext, but not a replacement for a hardware-backed keychain.
 */
class LocalEncryptedFileStore final : public UBAANext::ISecureStore {
public:
    explicit LocalEncryptedFileStore(std::filesystem::path file_path);
    ~LocalEncryptedFileStore() override;

    LocalEncryptedFileStore(const LocalEncryptedFileStore &) = delete;
    LocalEncryptedFileStore &operator=(const LocalEncryptedFileStore &) = delete;

    void set_string(const std::string &key, const std::string &value) override;
    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;
    void remove(const std::string &key) override;
    Result<void> flush() override;
    void clear() override;

private:
    void load_from_file();
    [[nodiscard]] Result<void> save_to_file() const;

    std::filesystem::path m_file_path;
    std::unordered_map<std::string, std::string> m_data;
    mutable bool m_dirty = false;
};

} // namespace UBAANext::Platform::Linux
