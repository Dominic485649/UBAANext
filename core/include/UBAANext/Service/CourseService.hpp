/**
 * @file CourseService.hpp
 * @brief 课程表查询服务
 *
 * 提供查询学生当天或指定周次课程表的方法。
 * 支持 mock 模式和真实 API 模式（VPN/内网）。
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class CourseService {
public:
    CourseService(IHttpClient &http_client, ICacheStore &cache);
    CourseService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::Course>> get_today_courses();
    Result<std::vector<Model::Course>> get_date_courses(const std::string &date);
    Result<std::vector<Model::Course>> get_week_courses(int week);
    Result<std::vector<Model::Course>> get_week_courses(int week, const std::string &term_code);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;

    [[nodiscard]] std::string resolve_url(const std::string &url) const;

    /// 从 byxt API 解析周课表
    Result<std::vector<Model::Course>> fetch_week_courses_real(int week, const std::string &term_code);
};

} // namespace UBAANext
