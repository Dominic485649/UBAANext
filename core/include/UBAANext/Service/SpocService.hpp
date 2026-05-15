#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Spoc.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {

struct SpocAssignmentQuery {
    bool pending_only = false;
    bool include_expired = false;
};

class SpocService {
public:
    SpocService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries();
    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries(const SpocAssignmentQuery &query);
    Result<Model::SpocAssignmentDetail> assignment_detail(const std::string &assignment_id);

    Result<std::vector<Model::FeatureRecord>> list_assignments();
    Result<std::vector<Model::FeatureRecord>> list_assignments(const SpocAssignmentQuery &query);
    Result<Model::FeatureRecord> show_assignment(const std::string &assignment_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_token;
    std::string m_role_code;

    Result<void> ensure_login(bool force_refresh = false);
    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries_once();
    Result<Model::SpocAssignmentDetail> assignment_detail_once(const std::string &assignment_id);
    Result<std::string> fetch_login_token();
    Result<void> perform_cas_login(const std::string &token);
    Result<nlohmann::json> get_envelope(const std::string &url);
    Result<nlohmann::json> post_envelope(const std::string &url, const nlohmann::json &body);
    Result<nlohmann::json> unwrap_envelope(const nlohmann::json &envelope, const std::string &body);
};

} // namespace UBAANext
