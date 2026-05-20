/**
 * @file Error.hpp
 * @brief UBAA Next 统一错误类型定义
 *
 * 定义整个代码库使用的 ErrorCode 枚举和 Error 结构体，
 * 作为 Result<T> 的错误类型参数。
 */
#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace UBAANext {

/**
 * @brief UBAA Next 操作的分类错误码
 *
 * 作为 Error 结构体的判别标识，使调用方能够
 * 根据错误类别进行分支处理，无需字符串比较。
 */
enum class ErrorCode {
    None,              ///< 无错误（默认状态）
    Unknown,           ///< 未分类错误的兜底值
    InvalidArgument,   ///< 调用方提供了无效参数
    NetworkError,      ///< HTTP / 网络连接失败
    AuthFailed,        ///< 认证或凭据错误
    SessionExpired,    ///< 已存储的会话令牌不再有效
    ParseError,        ///< 响应解析失败（JSON 等）
    UnsupportedPlatform,          ///< 当前平台不支持该能力
    UnsupportedNetwork,           ///< 当前平台未接入真实网络能力
    UnsupportedSecureStore,       ///< 当前平台未接入安全存储能力
    UnsupportedCrypto,            ///< 当前平台未接入真实加密能力
    UnsupportedCookiePersistence, ///< 当前平台未接入安全 Cookie 持久化
    Timeout,                      ///< 网络或平台操作超时
    TlsError,                     ///< TLS/证书校验失败
    CryptoError,                  ///< 加密、摘要或签名操作失败
    StorageError,                 ///< 安全存储或本地状态读写失败
    NotImplemented     ///< 功能预留，将在未来版本实现
};

/**
 * @brief 将 ErrorCode 转换为人类可读的字符串视图
 *
 * 此函数为 constexpr，当以常量参数调用时可在编译期求值。
 *
 * @param code 要转换的错误码
 * @return 描述该错误码的 string_view
 */
constexpr std::string_view error_code_to_string(ErrorCode code) {
    switch (code) {
    case ErrorCode::None:
        return "None";
    case ErrorCode::Unknown:
        return "Unknown";
    case ErrorCode::InvalidArgument:
        return "InvalidArgument";
    case ErrorCode::NetworkError:
        return "NetworkError";
    case ErrorCode::AuthFailed:
        return "AuthFailed";
    case ErrorCode::SessionExpired:
        return "SessionExpired";
    case ErrorCode::ParseError:
        return "ParseError";
    case ErrorCode::UnsupportedPlatform:
        return "UnsupportedPlatform";
    case ErrorCode::UnsupportedNetwork:
        return "UnsupportedNetwork";
    case ErrorCode::UnsupportedSecureStore:
        return "UnsupportedSecureStore";
    case ErrorCode::UnsupportedCrypto:
        return "UnsupportedCrypto";
    case ErrorCode::UnsupportedCookiePersistence:
        return "UnsupportedCookiePersistence";
    case ErrorCode::Timeout:
        return "Timeout";
    case ErrorCode::TlsError:
        return "TlsError";
    case ErrorCode::CryptoError:
        return "CryptoError";
    case ErrorCode::StorageError:
        return "StorageError";
    case ErrorCode::NotImplemented:
        return "NotImplemented";
    }
    // 不设置 default 分支：让编译器在枚举新增值时发出警告
    return "Unknown";
}

/**
 * @brief 通用错误容器，携带分类代码和描述消息
 *
 * 作为 Result<T> 的错误类型参数。
 * 使用 ErrorCode 和描述性消息构造。
 *
 * 使用示例：
 * @code
 *   Result<int> divide(int a, int b) {
 *       if (b == 0) return make_error(ErrorCode::InvalidArgument, "除以零");
 *       return a / b;
 *   }
 * @endcode
 */
struct Error {
    ErrorCode code = ErrorCode::None;  ///< 错误分类标识
    std::string message;               ///< 人类可读的错误描述

    /// 默认构造一个 ErrorCode::None 的错误对象
    Error() = default;

    /**
     * @brief 使用错误码和消息构造错误对象
     * @param c   错误分类标识
     * @param msg 描述性消息（将被移动到结构体内部）
     */
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}

    /**
     * @brief 检查此错误是否代表实际的失败
     * @return 如果 code != ErrorCode::None 则返回 true
     */
    [[nodiscard]] explicit operator bool() const { return code != ErrorCode::None; }
};

} // namespace UBAANext