/**
 * @file Account.hpp
 * @brief 学生账户数据模型
 *
 * 表示已登录用户的身份信息和认证令牌。
 * 由 AuthService 在登录成功后填充，
 * 通过 SecureStoreAdapter / SessionManager 进行持久化。
 */
#pragma once

#include <string>

namespace UBAANext::Model {

/**
 * @brief 包含身份和令牌信息的学生账户
 *
 * 字段映射到 UBAA 认证 API 的 LoginResponse DTO。
 */
struct Account {
    std::string student_id;     ///< 学号（例如 "20260000"）
    std::string display_name;   ///< UBAA 系统中的显示名称
    std::string access_token;   ///< 用于 API 调用的 JWT 访问令牌
    std::string refresh_token;  ///< 用于令牌刷新的 JWT 刷新令牌
};

} // namespace UBAANext::Model
