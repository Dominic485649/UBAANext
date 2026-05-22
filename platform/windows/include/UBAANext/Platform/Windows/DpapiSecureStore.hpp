#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace UBAANext::Platform::Windows {

class DpapiSecureStore final : public UBAANext::ISecureStore {
public:
    explicit DpapiSecureStore(std::filesystem::path file_path);
    ~DpapiSecureStore() override;

    DpapiSecureStore(const DpapiSecureStore &) = delete;
    DpapiSecureStore &operator=(const DpapiSecureStore &) = delete;

    void set_string(const std::string &key, const std::string &value) override;
    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;
    void remove(const std::string &key) override;
    Result<void> flush() override;
    void clear() override;

private:
    void load_from_file();
    void save_to_file() const;

    std::filesystem::path m_file_path;
    std::unordered_map<std::string, std::string> m_data;
};

} // namespace UBAANext::Platform::Windows
