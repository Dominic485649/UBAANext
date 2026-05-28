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
#if UBAANEXT_ENABLE_MOCKS
    ExamService(IHttpClient &http_client, ICacheStore &cache);
#endif
    ExamService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: fetches exams for an explicit term; real mode rejects missing term_code and propagates session/parse errors. */
    Result<std::vector<Model::Exam>> get_exams(const std::string &term_code = "");

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
