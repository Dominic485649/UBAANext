#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Signin.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace UBAANext {

class SigninService {
public:
    SigninService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);
    SigninService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, std::string student_id);

    /** ReadOnlyCandidate: fetches today's sign-in courses through the iClass session path. */
    Result<std::vector<Model::SigninCourse>> list_today_courses();
    /** ReadOnlyCandidate: fetches one explicit date's sign-in schedule through the iClass session path. */
    Result<std::vector<Model::SigninCourse>> list_schedule(const std::string &date);
    /** ReadOnlyCandidate: fetches all sign-in courses for one term. */
    Result<std::vector<Model::SigninTermCourse>> list_term_courses(const std::string &term_code);
    /** ReadOnlyCandidate: fetches all sign-in schedule details for one course id. */
    Result<std::vector<Model::SigninCourse>> list_course_schedule(const std::string &course_id);

    /** ReadOnlyCandidate: returns CLI/UI feature records for today's sign-in state. */
    Result<std::vector<Model::FeatureRecord>> list_today();
    /** ReadOnlyCandidate: returns CLI/UI records for an explicit date's sign-in schedule. */
    Result<std::vector<Model::FeatureRecord>> schedule_records(const std::string &date);
    /** ReadOnlyCandidate: returns CLI/UI records for all term courses. */
    Result<std::vector<Model::FeatureRecord>> term_course_records(const std::string &term_code);
    /** ReadOnlyCandidate: returns CLI/UI records for one course's sign-in details. */
    Result<std::vector<Model::FeatureRecord>> course_schedule_records(const std::string &course_id);

    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);

    /**
     * WriteGated remote mutation: yes. Requires an explicit write gate and a course id;
     * default platform capabilities keep this operation fail-closed.
     */
    Result<Model::MutationResult> perform_signin(const std::string &course_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    WriteOperationGate m_write_gate = disabled_write_operation("signin do");
    std::string m_student_id;
    std::string m_user_id;
    std::string m_session_id;

    Result<std::string> get_student_id();
    Result<std::string> resolve_login_name();
    Result<std::pair<std::string, std::string>> login_iclass(const std::string &login_name);
    Result<std::pair<std::string, std::string>> ensure_iclass_session(bool force_refresh = false);
    Result<std::vector<Model::SigninCourse>> list_today_courses(bool allow_retry);
    Result<std::vector<Model::SigninCourse>> list_schedule(const std::string &date, bool allow_retry);
    Result<std::vector<Model::SigninTermCourse>> list_term_courses(const std::string &term_code, bool allow_retry);
    Result<std::vector<Model::SigninCourse>> list_course_schedule(const std::string &course_id, bool allow_retry);
    Result<Model::MutationResult> perform_signin_once(const std::string &course_id, bool allow_retry);
};

} // namespace UBAANext
