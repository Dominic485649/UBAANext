/**
 * @file Session.cpp
 * @brief 内存会话状态的实现
 */

#include <UBAANext/Auth/Session.hpp>

namespace UBAANext {

void Session::set_account(Model::Account account) {
    // 将账户对象移动到内部可选值中
    m_account = std::move(account);
}

const std::optional<Model::Account> &Session::account() const {
    // 返回内部可选账户的常量引用
    return m_account;
}

bool Session::is_valid() const {
    // 通过检查可选值是否有值来判断会话是否有效
    return m_account.has_value();
}

void Session::clear() {
    // 重置可选值，释放已存储的账户
    m_account.reset();
}

} // namespace UBAANext