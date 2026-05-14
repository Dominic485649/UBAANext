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
 * 常见 HTTP 状态码参考：
 *   - 200 OK                  : 请求成功
 *   - 201 Created             : 资源创建成功
 *   - 301 Moved Permanently   : 永久重定向
 *   - 400 Bad Request         : 请求参数错误
 *   - 401 Unauthorized        : 未认证（需要登录）
 *   - 403 Forbidden           : 无权限访问
 *   - 404 Not Found           : 资源不存在
 *   - 500 Internal Server Error : 服务器内部错误
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
     * 以键值对形式存储 HTTP 响应头。常见的响应头包括：
     *   - "Content-Type"        : 响应体的 MIME 类型（如 application/json）
     *   - "Content-Length"      : 响应体的字节长度
     *   - "Set-Cookie"          : 服务器要求客户端设置的 Cookie
     *   - "Location"            : 重定向目标 URL（配合 3xx 状态码使用）
     *
     * 注意：不同 HTTP 客户端实现可能对键名的大小写处理不同，
     * 建议使用大小写不敏感的方式查找。
     */
    std::unordered_map<std::string, std::string> headers;

    /**
     * @brief 响应体内容
     *
     * 服务器返回的响应正文。通常为 JSON 字符串（API 接口），
     * 但也可能是 HTML、纯文本或其他格式。
     *
     * 对于非文本响应（如文件下载），此字段可能包含二进制数据
     * 的字符串表示，但这不是推荐用法——大文件下载应使用流式处理。
     */
    std::string body;
};

} // namespace UBAANext
