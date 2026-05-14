/**
 * @file SecureStoreAdapter.cpp
 * @brief Account 序列化到 ISecureStore 的实现
 */

#include <UBAANext/Storage/SecureStoreAdapter.hpp>

namespace UBAANext {

SecureStoreAdapter::SecureStoreAdapter(ISecureStore &store) : m_store(store) {}

void SecureStoreAdapter::save_account(const Model::Account &account) {
    // 将 Account 的各字段分别存储到对应的键中
    m_store.set_string("account.student_id", account.student_id);
    m_store.set_string("account.display_name", account.display_name);
    m_store.set_string("account.access_token", account.access_token);
    m_store.set_string("account.refresh_token", account.refresh_token);
}

std::optional<Model::Account> SecureStoreAdapter::load_account() const {
    // 必需字段：student_id 和 display_name
    auto student_id = m_store.get_string("account.student_id");
    auto display_name = m_store.get_string("account.display_name");

    // 任一必需字段缺失则返回空
    if (!student_id || !display_name) {
        return std::nullopt;
    }

    Model::Account account;
    account.student_id = std::move(*student_id);
    account.display_name = std::move(*display_name);

    // 可选字段（旧版本会话可能不存在这些键）
    auto access_token = m_store.get_string("account.access_token");
    auto refresh_token = m_store.get_string("account.refresh_token");
    if (access_token) account.access_token = std::move(*access_token);
    if (refresh_token) account.refresh_token = std::move(*refresh_token);

    return account;
}

void SecureStoreAdapter::clear_account() {
    // 从存储中移除所有账户相关键
    m_store.remove("account.student_id");
    m_store.remove("account.display_name");
    m_store.remove("account.access_token");
    m_store.remove("account.refresh_token");
}

} // namespace UBAANext