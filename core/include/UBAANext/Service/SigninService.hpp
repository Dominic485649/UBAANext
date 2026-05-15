#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class SigninService {
public:
    SigninService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::FeatureRecord>> list_today();
    Result<Model::MutationResult> perform_signin(const std::string &course_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;

    Result<std::string> get_student_id();
    Result<std::pair<std::string, std::string>> login_iclass(const std::string &student_id);
};

} // namespace UBAANext
