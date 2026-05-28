/**
 * @file ExitCodes.hpp
 * @brief CLI 统一退出码定义
 *
 * 按 CLI 命令 API 合同定义进程退出码，
 * 所有命令处理函数返回 ExitCode 枚举，
 * 由 main() 映射为进程退出码。
 */
#pragma once

#include <cstdint>

namespace UBAANextCli {

enum class ExitCode : int {
    Ok = 0,              ///< 成功
    General = 1,         ///< 通用失败
    InvalidArgument = 2, ///< 参数错误
    AuthRequired = 3,    ///< 未登录或会话过期
    Network = 4,         ///< 网络错误
    Parse = 5,           ///< 解析错误
    Storage = 6,         ///< 安全存储错误
};

} // namespace UBAANextCli
