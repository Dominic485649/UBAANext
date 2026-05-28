/**
 * @file HttpResponse.hpp
 * @brief HTTP 响应数据结构定义
 *
 * 本文件定义了 UBAANext 项目中使用的平台无关 HTTP 响应结构。
 * HttpResponse 由 IHttpClient::send() 方法返回，封装了服务器
 * 对 HTTP 请求的完整响应信息，包括状态码、响应头和响应体。
 *
 * 设计要点：
 *   - 状态码使用 int 类型存储，兼容所有 HTTP 标准状态码
 *   - 响应头使用 unordered_map 存储，便于按名称快速检索
 *   - 响应体使用 std::string 存储，适用于 JSON、HTML 等文本响应
 *   - 所有字段提供零值/空值默认，确保构造后对象处于有效状态
 *
 * @author UBAANext Team
 * @version 0.2
 */
#pragma once

#include <string>
#include <unordered_map>

namespace UBAANext {

/**
 * @brief 平台无关的 HTTP 响应结构
 *
 * 封装了 HTTP 服务器返回的完整响应信息。该结构体由 IHttpClient
 * 接口的实现类在执行 HttpRequest 后填充并返回给调用方。
 *
 * 使用示例：
 * @code
 *   HttpRequest req;
 *   req.method = HttpMethod::Get;
 *   req.url    = "https://api.example.com/status";
 *
 *   HttpResponse resp = httpClient->send(req);
 *
 *   if (resp.status_code == 200) {
 *       // 请求成功，解析 resp.body 中的 JSON 数据
 *       auto contentType = resp.headers["Content-Type"];
 *       // ...
 *   } else {
 *       // 处理错误（如 404 Not Found、500 Internal Server Error）
 *   }
 * @endcode
 *
 * Sensitive output：status code、headers 和 body 可能包含 session/cookie/token、HTML 或业务数据。
 * 调用方必须通过 redaction-aware 错误和输出路径处理，不能直接记录原始响应。
 */
struct HttpResponse {
    /**
     * @brief HTTP 状态码
     *
     * 服务器返回的标准 HTTP 状态码。常见值为 200（成功）、
     * 404（未找到）、500（服务器错误）等。
     *
     * 默认初始化为 0，表示尚未收到响应或请求失败。
     * 上层代码应检查此值以确定请求是否成功。
     */
    int status_code = 0;

    /**
     * @brief 响应头键值对
     *
     * Sensitive output：Set-Cookie、Location、Authorization-like headers 不得直接输出。
     */
    std::unordered_map<std::string, std::string> headers;

    /**
     * @brief Sensitive output：响应正文可能是 JSON、HTML、token 或敏感业务数据，不得原样写入日志。
     */
    std::string body;
};

} // namespace UBAANext
