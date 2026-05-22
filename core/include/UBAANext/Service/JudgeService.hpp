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

    Result<std::vector<Model::JudgeAssignmentSummary>> list_assignment_summaries(const std::string &course_id);
    Result<std::vector<Model::JudgeAssignmentSummary>> list_assignment_summaries(const JudgeAssignmentQuery &query);
    Result<Model::JudgeAssignmentDetail> assignment_detail(const std::string &assignment_id);
    Result<std::vector<Model::JudgeAssignmentDetail>> assignment_details_batch(const std::vector<std::string> &assignment_ids);

    Result<std::vector<Model::FeatureRecord>> list_assignments(const std::string &course_id);
    Result<std::vector<Model::FeatureRecord>> list_assignments(const JudgeAssignmentQuery &query);
    Result<Model::FeatureRecord> show_assignment(const std::string &assignment_id);
    Result<Model::FeatureRecord> show_assignment_details(const std::string &assignment_id);
    Result<std::vector<Model::FeatureRecord>> show_assignment_details_batch(const std::vector<std::string> &assignment_ids);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    bool m_session_activated = false;

    Result<void> ensure_session(bool force_refresh = false);
    Result<std::string> get_html(const std::string &url);
};

} // namespace UBAANext
