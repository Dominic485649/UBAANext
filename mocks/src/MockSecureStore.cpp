/**
 * @file MockSecureStore.cpp
 * @brief 模拟安全存储的实现
 */

#include <UBAANextMocks/MockSecureStore.hpp>

namespace UBAANextMocks {

void MockSecureStore::set_string(const std::string &key, const std::string &value) {
    // 使用赋值操作存储或覆盖键值对
    m_data[key] = value;
}

std::optional<std::string> MockSecureStore::get_string(const std::string &key) const {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MockSecureStore::remove(const std::string &key) {
    m_data.erase(key);
}

void MockSecureStore::clear() {
    m_data.clear();
}

} // namespace UBAANextMocks
