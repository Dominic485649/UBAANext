#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

namespace UBAANext::Platform::Harmony {

class UnsupportedSecureStore final : public UBAANext::ISecureStore {
public:
    void set_string(const std::string &key, const std::string &value) override;
    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;
    void remove(const std::string &key) override;
    void clear() override;
};

} // namespace UBAANext::Platform::Harmony
