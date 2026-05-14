/**
 * @file AppContext.hpp
 * @brief CLI 运行上下文
 *
 * 聚合运行模式、基础设施实例，避免每个命令重复创建。
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>

#include "CliConfig.hpp"

#include <memory>

namespace UBAANextCli {

/**
 * @brief CLI 运行上下文，持有所有共享基础设施
 *
 * 生命周期覆盖一次 CLI 调用的全程。
 * 命令处理函数通过 AppContext 获取 http client / cache / store，
 * 无需关心具体实现类型。
 */
struct AppContext {
    bool mock_mode = true;                                      ///< 是否使用 mock 实现
    UBAANext::ConnectionMode conn_mode = UBAANext::ConnectionMode::WebVPN;  ///< 连接模式
    CliConfig config;                                           ///< CLI 配置
    std::unique_ptr<UBAANext::IHttpClient> http;                ///< HTTP 客户端
    std::unique_ptr<UBAANext::ICacheStore> cache;               ///< 缓存存储
    std::unique_ptr<UBAANext::ISecureStore> store;              ///< 安全存储（开发态为 PlainFileStore）
};

} // namespace UBAANextCli
