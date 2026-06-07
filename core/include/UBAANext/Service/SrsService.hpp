#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Srs.hpp>
#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>

#include <optional>
#include <string>
#include <vector>

namespace UBAANext {

class SrsService {
public:
    SrsService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::FeatureRecord>> config();
    Result<Model::FeatureRecord> batch();
    Result<std::vector<Model::FeatureRecord>> courses(const Model::SrsCourseFilter &filter);
    Result<std::vector<Model::FeatureRecord>> preselected();
    Result<std::vector<Model::FeatureRecord>> selected();

    void set_write_operation_gate(WriteOperationGate gate);
    Result<Model::MutationResult> preselect_course(const Model::SrsCourseOperation &operation);
    Result<Model::MutationResult> select_course(const Model::SrsCourseOperation &operation);
    Result<Model::MutationResult> drop_course(const Model::SrsCourseOperation &operation);

private:
    IHttpClient &m_http_client;
    ICookieStore *m_cookie_store = nullptr;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::optional<std::string> m_token;
    WriteOperationGate m_write_gate = disabled_write_operation("srs write");

    Result<std::string> token(bool force_refresh = false);
    Result<HttpResponse> send_srs_request(HttpRequest request, const std::string &context, bool token_query = false);
};

} // namespace UBAANext
