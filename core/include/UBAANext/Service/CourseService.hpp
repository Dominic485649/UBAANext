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
#if UBAANEXT_ENABLE_MOCKS
    CourseService(IHttpClient &http_client, ICacheStore &cache);
#endif
    CourseService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: returns today's courses from cache/mock or the real schedule API; field drift remains possible. */
    Result<std::vector<Model::Course>> get_today_courses();
    /** ReadOnlyCandidate: remote read for a specific date; unsupported modes fail explicitly. */
    Result<std::vector<Model::Course>> get_date_courses(const std::string &date);
    /** PartiallyMigrated: mock can infer week-only data, but real mode requires term_code and fails with InvalidArgument. */
    Result<std::vector<Model::Course>> get_week_courses(int week);
    /** ReadOnlyCandidate: fetches week courses for an explicit term; live BYXT fields remain unverified. */
    Result<std::vector<Model::Course>> get_week_courses(int week, const std::string &term_code);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;

    [[nodiscard]] std::string resolve_url(const std::string &url) const;

    /** ReadOnlyCandidate helper: fetches real BYXT week courses; propagates activation, network, auth, and parse failures. */
    Result<std::vector<Model::Course>> fetch_week_courses_real(int week, const std::string &term_code);
};

} // namespace UBAANext
