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

    /** ReadOnlyCandidate: lists SPOC assignments after CAS/token activation; live role/token drift remains possible. */
    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries();
    /** ReadOnlyCandidate: filtered SPOC assignment list; pending/expired semantics remain partially migrated. */
    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries(const SpocAssignmentQuery &query);
    /** Sensitive output: assignment detail may include homework metadata and must stay behind redaction-aware output. */
    Result<Model::SpocAssignmentDetail> assignment_detail(const std::string &assignment_id);

    /** ReadOnlyCandidate: returns SPOC assignments as FeatureRecord for CLI/Todo consumers. */
    Result<std::vector<Model::FeatureRecord>> list_assignments();
    /** ReadOnlyCandidate: filtered FeatureRecord projection; parser/session drift remains possible. */
    Result<std::vector<Model::FeatureRecord>> list_assignments(const SpocAssignmentQuery &query);
    /** Sensitive output: single SPOC assignment record; missing ids are InvalidArgument. */
    Result<Model::FeatureRecord> show_assignment(const std::string &assignment_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_token;
    std::string m_role_code;

    /** PartiallyMigrated session activation: stores transient SPOC token/role in memory and must propagate SessionExpired. */
    Result<void> ensure_login(bool force_refresh = false);
    /** Sensitive output: lists assignment summaries once after login; raw envelope data must not be logged. */
    Result<std::vector<Model::SpocAssignmentSummary>> list_assignment_summaries_once();
    /** Sensitive output: fetches one assignment detail after login; parser drift remains possible. */
    Result<Model::SpocAssignmentDetail> assignment_detail_once(const std::string &assignment_id);
    /** Sensitive output: fetches login token through CAS redirects; token must not be logged or persisted as secure storage. */
    Result<std::string> fetch_login_token();
    /** Sensitive input: performs CAS login with a transient token and returns SessionExpired on login pages. */
    Result<void> perform_cas_login(const std::string &token);
    /** ReadOnlyCandidate helper: fetches a SPOC JSON envelope and retries through ensure_login on session expiry. */
    Result<nlohmann::json> get_envelope_once(const std::string &url);
    /** Sensitive input: posts a SPOC JSON envelope; body may include encrypted query state and must not be logged. */
    Result<nlohmann::json> post_envelope_once(const std::string &url, const nlohmann::json &body);
    /** ReadOnlyCandidate helper: GET wrapper with one login refresh retry. */
    Result<nlohmann::json> get_envelope(const std::string &url);
    /** Sensitive input: POST wrapper with one login refresh retry; body must stay redaction-aware. */
    Result<nlohmann::json> post_envelope(const std::string &url, const nlohmann::json &body);
    /** PartiallyMigrated envelope parser: maps SPOC session and backend failures to stable errors. */
    Result<nlohmann::json> unwrap_envelope(const nlohmann::json &envelope, const std::string &body);
};

} // namespace UBAANext
