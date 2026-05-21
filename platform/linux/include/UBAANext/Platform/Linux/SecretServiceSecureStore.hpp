#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

namespace UBAANext::Platform::Linux {

class SecretServiceSecureStore final : public UBAANext::ISecureStore {
public:
    explicit SecretServiceSecureStore(std::string collection = "default");

    void set_string(const std::string &key, const std::string &value) override;
    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;
    void remove(const std::string &key) override;
    void clear() override;

private:
    std::string m_collection;
};

} // namespace UBAANext::Platform::Linux
