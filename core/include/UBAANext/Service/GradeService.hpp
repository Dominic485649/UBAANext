#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class GradeService {
public:
    GradeService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** Sensitive output: ReadOnlyCandidate term-specific grades; real mode requires explicit term_code and propagates session errors. */
    Result<std::vector<Model::Grade>> list_grades(const std::string &term_code);
    /** PartiallyMigrated: mock can list all grades, but real mode fails with InvalidArgument until term iteration is proven. */
    Result<std::vector<Model::Grade>> list_all_grades();

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
