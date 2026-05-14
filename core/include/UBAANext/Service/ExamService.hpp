/**
 * @file ExamService.hpp
 * @brief 考试查询服务
 *
 * 提供查询学生即将进行的考试的方法。
 * 通过 Mock HTTP + JSON 解析获取数据，支持缓存。
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class ExamService {
public:
    ExamService(IHttpClient &http_client, ICacheStore &cache);
    ExamService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::Exam>> get_exams(const std::string &term_code = "2025-2026-2");

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
