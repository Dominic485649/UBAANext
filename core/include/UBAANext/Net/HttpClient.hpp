/**
 * @file HttpClient.hpp
 * @brief 抽象 HTTP 客户端接口定义
 *
 * 本文件定义了 UBAANext 项目中所有 HTTP 通信的抽象接口 IHttpClient。
 * 采用接口抽象的设计模式（依赖倒置原则），使上层业务代码不依赖于
 * 具体的 HTTP 传输实现，从而实现：
 *   - **可替换性**：可随时切换底层实现（如从 libcurl 切换到 WinHTTP）
 *   - **可测试性**：通过 Mock 实现进行单元测试，无需真正的网络请求
 *   - **平台无关性**：不同平台可提供不同的 IHttpClient 实现
 *
 * 实现此接口的类包括但不限于：
 *   - CurlHttpClient  : 基于 libcurl 的跨平台实现（生产环境使用）
 *   - MockHttpClient  : 用于单元测试的 Mock 实现
 *
 * @author UBAANext Team
 * @version 0.2
 */
#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpRequest.hpp>
#include <UBAANext/Net/HttpResponse.hpp>

namespace UBAANext {

/**
 * @brief 抽象 HTTP 客户端接口
 *
 * UBAANext 中所有的 HTTP 通信都通过此接口进行，以支持依赖注入和可测试性。
 * 上层 Service 类在构造时接收 IHttpClient 指针（或智能指针），
 * 调用 send() 方法发起 HTTP 请求，而不关心底层使用何种网络库。
 *
 * 依赖注入示例：
 * @code
 *   // 生产环境注入真实实现
 *   auto httpClient = std::make_unique<CurlHttpClient>();
 *   UserService userService(std::move(httpClient));
 *
 *   // 测试环境注入 Mock 实现
 *   auto mockClient = std::make_unique<MockHttpClient>();
 *   mockClient->set_response(HttpResponse{200, {}, R"({"user":"admin"})"});
 *   UserService userService(std::move(mockClient));
 * @endcode
 *
 * 线程安全性：
 *   - IHttpClient 接口本身不保证线程安全性
 *   - 具体实现类应在其文档中说明其线程安全级别
 *   - 如果需要在多线程环境中使用，调用方应自行加锁或使用
 *     线程安全的包装器
 */
class IHttpClient {
public:
    /**
     * @brief 虚析构函数
     *
     * 确保通过基类指针删除派生类对象时能正确调用派生类析构函数，
     * 防止资源泄漏。这是 C++ 中定义基类接口的标准做法。
     */
    virtual ~IHttpClient() = default;

    /**
     * @brief Sensitive transport boundary: 执行 HTTP 请求，可能发起真实远端请求。
     *
     * Mock 实现只验证合同；真实实现必须保持 URL/header/body/cookie/token 的错误脱敏。
     * @param request 完整填充的 HTTP 请求对象，包含方法、URL、请求头和请求体
     * @return 成功时包含服务器返回的 HTTP 响应；失败时包含稳定错误码
     */
    [[nodiscard]] virtual Result<HttpResponse> send(const HttpRequest &request) = 0;
};

} // namespace UBAANext
