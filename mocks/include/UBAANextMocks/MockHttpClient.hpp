/**
 * @file MockHttpClient.hpp
 * @brief 用于单元测试的模拟 HTTP 客户端
 *
 * 根据请求 URL 路由返回预设的 JSON 响应。
 * 预设了课程、考试、教室、学期、周次的 mock 数据。
 * 未匹配的 URL 返回空 "{}"。
 */
#pragma once

#include <UBAANext/Net/HttpClient.hpp>

#include <map>
#include <optional>
#include <string>

namespace UBAANextMocks {

/**
 * @brief 基于 URL 路由的模拟 HTTP 客户端
 *
 * 预设 mock 响应覆盖常见的 UBAA API 路径。
 * 可通过 set_mock_response() 添加自定义路由。
 * 可通过 set_network_error() / set_http_error() 模拟错误场景。
 */
class MockHttpClient : public UBAANext::IHttpClient {
public:
    MockHttpClient();

    /** @brief 根据请求 URL 返回对应的 JSON 响应 */
    [[nodiscard]] UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override;

    /**
     * @brief 添加或覆盖一个 URL 模式的 mock 响应
     * @param url_pattern URL 路径模式（如 "/schedule/today"）
     * @param json_body   要返回的 JSON 响应体
     */
    void set_mock_response(const std::string &url_pattern, std::string json_body);

    /**
     * @brief 设置指定 URL 的网络错误（send 返回 NetworkError）
     * @param url_pattern URL 路径模式
     * @param error_msg   错误消息
     */
    void set_network_error(const std::string &url_pattern, std::string error_msg);

    /**
     * @brief 设置指定 URL 的 HTTP 错误状态码
     * @param url_pattern URL 路径模式
     * @param status_code HTTP 状态码（如 401、403、500）
     * @param body        响应体
     */
    void set_http_error(const std::string &url_pattern, int status_code, std::string body);

    [[nodiscard]] int request_count(const std::string &url_pattern) const;

private:
    std::map<std::string, std::string> m_responses;
    std::map<std::string, int> m_request_counts;

    struct ErrorConfig {
        std::optional<std::string> network_error;
        std::optional<int> http_status;
        std::string http_body;
    };
    std::map<std::string, ErrorConfig> m_errors;
};

} // namespace UBAANextMocks
