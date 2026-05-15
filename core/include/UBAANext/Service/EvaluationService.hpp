#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class EvaluationService {
public:
    EvaluationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::FeatureRecord>> list_evaluations();
    Result<Model::MutationResult> submit_evaluations(const std::string &target_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    bool m_activated = false;

    Result<void> activate_session();
    Result<nlohmann::json> request_json(HttpMethod method, const std::string &url, const nlohmann::json &body = nlohmann::json{});
    Result<std::string> current_xnxq();
};

} // namespace UBAANext
