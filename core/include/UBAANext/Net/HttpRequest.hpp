/**
 * @file HttpRequest.hpp
 * @brief HTTP 请求数据结构定义
 *
 * 本文件定义了 UBAANext 项目中使用的平台无关 HTTP 请求结构。
 * HttpRequest 作为数据传输对象（DTO），由各业务 Service 类填充后
 * 传递给 IHttpClient::send() 方法执行实际的网络请求。
 *
 * 设计要点：
 *   - 使用枚举类 HttpMethod 限定支持的 HTTP 方法，避免无效方法
 *   - 请求头使用 unordered_map 存储，方便按名称快速查找和覆盖
 *   - 请求体使用 std::string 存储，适用于 JSON、表单数据等文本格式
 *   - 所有字段提供合理的默认值，开箱即用
 *
 * @author UBAANext Team
 * @version 0.2
 */
#pragma once

#include <UBAANext/Net/RedirectController.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace UBAANext {

/**
 * @brief 支持的 HTTP 方法枚举
 *
 * 列举了项目中实际使用到的四种标准 HTTP 方法。
 * 使用 enum class 保证类型安全，防止隐式转换为整数。
 *
 * 典型用法：
 *   - Get    ：查询数据（如获取用户信息、设备状态）
 *   - Post   ：提交数据（如登录认证、发送命令）
 *   - Put    ：更新数据（如修改设备配置）
 *   - Delete ：删除数据（如移除设备绑定）
 */
enum class HttpMethod {
    Get,     ///< HTTP GET 请求 —— 用于获取资源，参数通过 URL 查询字符串传递
    Post,    ///< HTTP POST 请求 —— 用于提交数据，参数通过请求体传递
    Put,     ///< HTTP PUT 请求 —— 用于更新资源，参数通过请求体传递
    Delete   ///< HTTP DELETE 请求 —— 用于删除指定资源
};

/**
 * @brief 平台无关的上传部件。
 *
 * Placeholder/Upload boundary：App/Shell 层负责读取本地文件、权限处理和 MIME 推断；HTTP adapter 只发送已经准备好的 bytes。
 * Sensitive input：filename 和 bytes 不得写入日志；存在该结构不代表业务上传 API 已实现。
 */
struct HttpUploadPart {
    std::string field_name;
    std::string filename;
    std::string content_type;
    std::vector<unsigned char> bytes;
};

/**
 * @brief 单次 HTTP 请求的传输选项。
 *
 * PartiallyMigrated transport boundary：字段为 0 / 空字符串时表示使用平台 adapter 默认值。
 * `redact_url_query_in_errors` 必须默认开启，错误路径不得泄露 query 中的 token/captcha/session。
 */
struct HttpTransportOptions {
    int connect_timeout_ms = 0;
    int request_timeout_ms = 0;
    std::string proxy;
    bool tls_verify_peer = true;
    bool tls_verify_host = true;
    bool redact_url_query_in_errors = true;
};

/**
 * @brief 平台无关的 HTTP 请求结构
 *
 * 封装了一次完整的 HTTP 请求所需的全部信息，包括请求方法、目标 URL、
 * 请求头和请求体。该结构体由上层业务代码（Service 类）构造，
 * 然后传递给 IHttpClient 接口的 send() 方法执行。
 *
 * 使用示例：
 * @code
 *   HttpRequest req;
 *   req.method  = HttpMethod::Post;
 *   req.url     = "https://api.example.com/login";
 *   req.headers["Content-Type"] = "application/json";
 *   req.body    = R"({"username":"admin","password":"123456"})";
 *
 *   HttpResponse resp = httpClient->send(req);
 * @endcode
 *
 * Sensitive input：url query、headers、body 和 multipart bytes 可能包含 credentials、cookie、token、captcha 或业务数据，
 * 错误、日志和 diagnostics 必须脱敏。HTTP method 存在不代表该请求是安全只读，调用方按业务 service gate 判断。
 */
struct HttpRequest {
    /**
     * @brief HTTP 请求方法
     *
     * 默认为 GET 请求。根据业务需要可设置为 Post、Put 或 Delete。
     */
    HttpMethod method = HttpMethod::Get;

    /**
     * @brief 完整的请求 URL
     *
     * 包含协议、主机名、端口（可选）和路径。
     * 示例："https://api.example.com:8080/v1/devices"
     */
    std::string url;

    /**
     * @brief 请求头键值对
     *
     * Sensitive input：Authorization、Cookie、Set-Cookie 派生值和 token-like header 不得直接输出。
     */
    std::unordered_map<std::string, std::string> headers;

    /** Sensitive input: request body may contain credentials, captcha, tokens, or write-operation payloads. */
    std::string body;

    /** Placeholder/Upload boundary: preloaded bytes only; this structure never reads local files itself. */
    std::vector<HttpUploadPart> multipart_parts;

    /** PartiallyMigrated transport settings; redaction defaults must remain fail-safe. */
    HttpTransportOptions transport;

    /** Redirect/session boundary: automatic redirects may expose session transitions and must keep URLs redacted. */
    RedirectOptions redirect;
};

} // namespace UBAANext
