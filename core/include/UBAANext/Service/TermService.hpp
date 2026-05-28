/**
 * @file TermService.hpp
 * @brief 学期/周次查询服务
 *
 * 提供查询学期列表和教学周列表的方法。
 * 通过 Mock HTTP + JSON 解析获取数据，支持缓存。
 */
#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class TermService {
public:
#if UBAANEXT_ENABLE_MOCKS
    TermService(IHttpClient &http_client, ICacheStore &cache);
#endif
    TermService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: lists terms through mock cache or real BYXT API; unsupported modes fail explicitly. */
    Result<std::vector<Model::Term>> get_terms();
    /** ReadOnlyCandidate: lists teaching weeks for an explicit term; missing term_code fails with InvalidArgument. */
    Result<std::vector<Model::Week>> get_weeks(const std::string &term_code);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
