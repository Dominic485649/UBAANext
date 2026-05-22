#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Signin.hpp>
#include <UBAANext/Net/HttpClient.hpp>
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

    Result<std::vector<Model::SigninCourse>> list_today_courses();
    Result<std::vector<Model::FeatureRecord>> list_today();
    Result<Model::MutationResult> perform_signin(const std::string &course_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_student_id;
    std::string m_user_id;
    std::string m_session_id;

    Result<std::string> get_student_id();
    Result<std::pair<std::string, std::string>> login_iclass(const std::string &student_id);
    Result<std::pair<std::string, std::string>> ensure_iclass_session(bool force_refresh = false);
    Result<std::vector<Model::SigninCourse>> list_today_courses(bool allow_retry);
    Result<Model::MutationResult> perform_signin_once(const std::string &course_id, bool allow_retry);
};

} // namespace UBAANext
