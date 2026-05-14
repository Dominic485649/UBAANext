/**
 * @file MockSecureStore.hpp
 * @brief 用于单元测试的内存模拟安全存储
 *
 * 使用 std::unordered_map 实现 ISecureStore。
 * 数据不会在实例间持久化。
 */
#pragma once

#include <UBAANext/Storage/SecureStore.hpp>

#include <string>
#include <unordered_map>

namespace UBAANextMocks {

/**
 * @brief ISecureStore 的内存模拟实现
 *
 * 所有数据存储在 unordered_map 中，
 * 对象销毁后数据即丢失。
 */
class MockSecureStore : public UBAANext::ISecureStore {
public:
    void set_string(const std::string &key, const std::string &value) override;

    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override;

    void remove(const std::string &key) override;

    void clear() override;

private:
    std::unordered_map<std::string, std::string> m_data;  ///< 内存存储
};

} // namespace UBAANextMocks
