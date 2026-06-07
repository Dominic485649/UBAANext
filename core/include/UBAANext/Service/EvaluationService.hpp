#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Evaluation.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class EvaluationService {
public:
    EvaluationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** PartiallyMigrated read path: fetches evaluation tasks; questionnaire/session drift remains possible. */
    Result<std::vector<Model::EvaluationTask>> list_evaluation_tasks();

    /** PartiallyMigrated read path exposed as FeatureRecord; failures must propagate to Todo partial failure. */
    Result<std::vector<Model::FeatureRecord>> list_evaluations();

    /** Fully migrated read path: fetches the questionnaire form and submit context for a pending task/course. */
    Result<Model::EvaluationForm> get_form(const std::string &target_id);

    /** Fully migrated read path exposed as FeatureRecord for CLI/ABI friendly output. */
    Result<Model::FeatureRecord> form_record(const std::string &target_id);

    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);

    /**
     * WriteGated remote mutation: yes. Submitting evaluations can be irreversible and requires the gate;
     * default platform capabilities keep this operation fail-closed.
     */
    Result<Model::MutationResult> submit_evaluations(const std::string &target_id);

    /** WriteGated remote mutation: submits one explicit evaluation form target with default or supplied answers. */
    Result<Model::MutationResult> submit_form(const Model::EvaluationSubmission &submission);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    WriteOperationGate m_write_gate = disabled_write_operation("evaluation submit");
    bool m_activated = false;

    Result<void> activate_session();
    Result<nlohmann::json> request_json(HttpMethod method, const std::string &url, const nlohmann::json &body = nlohmann::json{});
    Result<Model::EvaluationForm> request_form(const Model::EvaluationTask &task);
    Result<std::string> current_xnxq();
};

} // namespace UBAANext
