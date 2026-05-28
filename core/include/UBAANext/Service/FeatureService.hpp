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

    /**
     * ReadOnlyCandidate: returns the current cached/authenticated user summary.
     * Sensitive output: account identity fields must stay behind the normal redaction path.
     */
    Result<Model::FeatureRecord> user_info();

    /**
     * MockOnly/compatibility read router for legacy feature records.
     * Real UBAA semantics are not guaranteed for arbitrary domain/operation pairs.
     */
    Result<std::vector<Model::FeatureRecord>> list(const std::string &domain, const std::string &operation);

    /**
     * MockOnly/compatibility detail router for legacy feature records.
     * Unverified domain/operation pairs must not be treated as typed service coverage.
     */
    Result<Model::FeatureRecord> show(const std::string &domain, const std::string &operation, const std::string &id);

    /**
     * Placeholder write router: mock mode can return a contract result after confirmation;
     * real mode is UnsupportedPlatform and must not be used as a remote mutation entry.
     * Remote mutation: no in real mode.
     */
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
