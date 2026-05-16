#include <UBAANext/Service/TodoService.hpp>

#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/SpocService.hpp>

#include <utility>

namespace UBAANext {

namespace {

bool is_pending_todo(const std::string &source, const Model::FeatureRecord &record) {
    if (source == "signin") return record.status == "available";
    if (source == "evaluation") return record.status == "pending";
    if (source == "judge") return record.status == "available" || record.status == "unsubmitted" || record.status == "partial";
    if (source == "spoc") return record.status == "open" || record.status == "available" || record.status == "pending" || record.status == "unsubmitted";
    return record.status == "pending" || record.status == "available" || record.status == "open";
}

void normalize_todo_record(const std::string &source, Model::FeatureRecord &record) {
    record.fields["source"] = source;
    if (record.fields.find("type") == record.fields.end()) record.fields["type"] = source;
    if (record.fields.find("dueTime") == record.fields.end()) {
        auto deadline = record.fields.find("deadline");
        if (deadline != record.fields.end()) record.fields["dueTime"] = deadline->second;
    }
    if (record.fields.find("submissionStatus") == record.fields.end()) record.fields["submissionStatus"] = record.status;
}

void append_todo_records(std::vector<Model::FeatureRecord> &todos,
                         const std::string &source,
                         const std::vector<Model::FeatureRecord> &records,
                         bool pending_only) {
    for (auto record : records) {
        if (pending_only && !is_pending_todo(source, record)) continue;
        normalize_todo_record(source, record);
        todos.push_back(std::move(record));
    }
}

} // namespace

TodoService::TodoService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::vector<Model::FeatureRecord>> TodoService::list_todos(const TodoQuery &query) {
    std::vector<Model::FeatureRecord> todos;

#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        FeatureService service(m_http_client, m_cache, m_mode);
        if (auto spoc = service.list("spoc", "assignments")) append_todo_records(todos, "spoc", *spoc, query.pending_only);
        if (auto judge = service.list("judge", "assignments")) append_todo_records(todos, "judge", *judge, query.pending_only);
        if (auto signin = service.list("signin", "today")) append_todo_records(todos, "signin", *signin, query.pending_only);
        if (auto evaluation = service.list("evaluation", "list")) append_todo_records(todos, "evaluation", *evaluation, query.pending_only);
        return todos;
    }
#endif

    {
        SpocService service(m_http_client, m_cache, m_mode);
        if (auto spoc = service.list_assignments()) append_todo_records(todos, "spoc", *spoc, query.pending_only);
    }
    {
        JudgeService service(m_http_client, m_cache, m_mode);
        if (auto judge = service.list_assignments(JudgeAssignmentQuery{})) append_todo_records(todos, "judge", *judge, query.pending_only);
    }
    {
        SigninService service(m_http_client, m_cache, m_mode);
        if (auto signin = service.list_today()) append_todo_records(todos, "signin", *signin, query.pending_only);
    }
    {
        EvaluationService service(m_http_client, m_cache, m_mode);
        if (auto evaluation = service.list_evaluations()) append_todo_records(todos, "evaluation", *evaluation, query.pending_only);
    }

    return todos;
}

} // namespace UBAANext
