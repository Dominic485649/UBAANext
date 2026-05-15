#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/VenueReservation.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace UBAANext {

class VenueReservationService {
public:
    VenueReservationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::VenueSite>> venue_sites();
    Result<std::vector<Model::VenuePurposeType>> purpose_types();
    Result<std::vector<Model::VenueSpaceInfo>> day_spaces(const std::string &date, const std::string &site_id);
    Result<std::vector<Model::VenueOrder>> orders(int page = 1, int size = 20);
    Result<Model::VenueOrder> order_detail(const std::string &order_id);

    Result<std::vector<Model::FeatureRecord>> list_sites();
    Result<std::vector<Model::FeatureRecord>> list_purpose_types();
    Result<std::vector<Model::FeatureRecord>> day_info(const std::string &date, const std::string &site_id);
    Result<std::vector<Model::FeatureRecord>> list_orders(int page = 1, int size = 20);
    Result<Model::FeatureRecord> show_order(const std::string &order_id);
    Result<Model::FeatureRecord> lock_code();
    Result<Model::MutationResult> reserve(const std::string &site_id,
                                          const std::string &space_id,
                                          const std::string &date,
                                          const std::string &time_id,
                                          const std::string &purpose_type,
                                          const std::string &theme,
                                          const std::string &phone,
                                          const std::string &joiners,
                                          const std::string &captcha,
                                          const std::string &token);
    Result<Model::MutationResult> cancel_order(const std::string &order_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_access_token;

    Result<void> ensure_login(bool force_refresh = false);
    Result<nlohmann::json> request_json(HttpMethod method,
                                        const std::string &path,
                                        const std::map<std::string, std::string> &params = {},
                                        const std::map<std::string, std::string> &form = {},
                                        const std::map<std::string, std::string> &extra_headers = {},
                                        bool authorize = true,
                                        bool allow_retry = true);
};

} // namespace UBAANext
