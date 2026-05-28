#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Ygdk.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Upload/UploadPart.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace UBAANext {

class YgdkService {
public:
    YgdkService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: fetches YGDK overview and items; sensitive sports/health context should not be logged verbatim. */
    Result<std::pair<Model::YgdkOverview, std::vector<Model::YgdkItem>>> overview_data();
    /** ReadOnlyCandidate: lists clock-in records; location/time fields are sensitive output. */
    Result<std::vector<Model::YgdkRecord>> record_list(int page = 1, int size = 20);
    Result<std::vector<Model::FeatureRecord>> overview();
    Result<std::vector<Model::FeatureRecord>> records(int page = 1, int size = 20);
    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);
    /**
     * WriteGated remote mutation: yes. Submits a real clock-in with sensitive time/place/photo input;
     * default platform capabilities keep this operation fail-closed.
     */
    Result<Model::MutationResult> submit_clockin(const std::string &item_id,
                                                 const std::string &start_time,
                                                 const std::string &end_time,
                                                 const std::string &place,
                                                 bool share_to_square,
                                                 const UploadPart &photo);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    WriteOperationGate m_write_gate = disabled_write_operation("ygdk submit");
    std::string m_uid;
    std::string m_token;

    Result<void> ensure_session(bool force_refresh = false);
    Result<std::string> fetch_oauth_code();
    Result<nlohmann::json> post_form(const std::string &url,
                                     const std::map<std::string, std::string> &query = {},
                                     const std::map<std::string, std::string> &form = {},
                                     bool allow_retry = true);
};

} // namespace UBAANext
