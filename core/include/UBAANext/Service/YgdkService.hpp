#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Ygdk.hpp>
#include <UBAANext/Net/HttpClient.hpp>
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

    Result<std::pair<Model::YgdkOverview, std::vector<Model::YgdkItem>>> overview_data();
    Result<std::vector<Model::YgdkRecord>> record_list(int page = 1, int size = 20);
    Result<std::vector<Model::FeatureRecord>> overview();
    Result<std::vector<Model::FeatureRecord>> records(int page = 1, int size = 20);
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
    std::string m_uid;
    std::string m_token;

    Result<void> ensure_session(bool force_refresh = false);
    Result<std::string> fetch_oauth_code();
    Result<nlohmann::json> post_form(const std::string &url,
                                     const std::map<std::string, std::string> &query = {},
                                     const std::map<std::string, std::string> &form = {});
};

} // namespace UBAANext
