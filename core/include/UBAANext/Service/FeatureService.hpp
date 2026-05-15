#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class FeatureService {
public:
    FeatureService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<Model::FeatureRecord> user_info();
    Result<std::vector<Model::FeatureRecord>> list(const std::string &domain, const std::string &operation);
    Result<Model::FeatureRecord> show(const std::string &domain, const std::string &operation, const std::string &id);
    Result<Model::MutationResult> mutate(const std::string &domain,
                                         const std::string &operation,
                                         const std::string &id,
                                         bool confirmed);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;

#if UBAANEXT_ENABLE_MOCKS
    Result<std::vector<Model::FeatureRecord>> mock_list(const std::string &domain, const std::string &operation) const;
    Result<Model::FeatureRecord> mock_show(const std::string &domain, const std::string &operation, const std::string &id) const;
#endif
};

} // namespace UBAANext
