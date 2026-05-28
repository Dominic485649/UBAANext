#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Judge.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

struct JudgeAssignmentQuery {
    std::string course_id;
    bool include_expired = false;
    bool include_history = false;
};

class JudgeService {
public:
    JudgeService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: lists judge assignments for one course through the XiJi session path. */
    Result<std::vector<Model::JudgeAssignmentSummary>> list_assignment_summaries(const std::string &course_id);
    /** ReadOnlyCandidate: supports include_expired/include_history filters; HTML parser drift remains possible. */
    Result<std::vector<Model::JudgeAssignmentSummary>> list_assignment_summaries(const JudgeAssignmentQuery &query);
    /** Sensitive output: assignment detail may include problem/submission metadata and propagates missing-id errors. */
    Result<Model::JudgeAssignmentDetail> assignment_detail(const std::string &assignment_id);
    /** Sensitive output: batch details preserve successful items and return status=error records for per-item failures. */
    Result<std::vector<Model::JudgeAssignmentDetail>> assignment_details_batch(const std::vector<std::string> &assignment_ids);

    /** ReadOnlyCandidate: returns assignment summaries as FeatureRecord for CLI/Todo consumers. */
    Result<std::vector<Model::FeatureRecord>> list_assignments(const std::string &course_id);
    /** ReadOnlyCandidate: filtered FeatureRecord projection; parser and session drift remain possible. */
    Result<std::vector<Model::FeatureRecord>> list_assignments(const JudgeAssignmentQuery &query);
    /** Sensitive output: returns a single assignment FeatureRecord; missing ids are InvalidArgument. */
    Result<Model::FeatureRecord> show_assignment(const std::string &assignment_id);
    /** Sensitive output: returns detail FeatureRecord from the real XiJi detail path. */
    Result<Model::FeatureRecord> show_assignment_details(const std::string &assignment_id);
    /** Sensitive output: batch detail projection preserves successful records and status=error records for per-item failures. */
    Result<std::vector<Model::FeatureRecord>> show_assignment_details_batch(const std::vector<std::string> &assignment_ids);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    bool m_session_activated = false;

    /** PartiallyMigrated session activation: remote read login state is cached in-process and must propagate SessionExpired. */
    Result<void> ensure_session(bool force_refresh = false);
    /** Sensitive output: fetches raw HTML for parser entry and must not be logged verbatim. */
    Result<std::string> get_html(const std::string &url);
};

} // namespace UBAANext
