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

    /** ReadOnlyCandidate: returns CLI/UI feature records for today's sign-in state. */
    Result<std::vector<Model::FeatureRecord>> list_today();

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
    Result<Model::MutationResult> perform_signin_once(const std::string &course_id, bool allow_retry);
};

} // namespace UBAANext
