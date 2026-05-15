#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class LibrarySeatService {
public:
    LibrarySeatService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    Result<std::vector<Model::FeatureRecord>> list_libraries(const std::string &day);
    Result<std::vector<Model::FeatureRecord>> list_areas(const std::string &library_id, const std::string &day, const std::string &storey_id = "");
    Result<Model::FeatureRecord> show_area(const std::string &area_id);
    Result<std::vector<Model::FeatureRecord>> list_seats(const std::string &area_id, const std::string &day, const std::string &start_time = "08:00", const std::string &end_time = "22:00");
    Result<std::vector<Model::FeatureRecord>> list_reservations(int page = 1, int limit = 20);
    Result<Model::MutationResult> reserve_seat(const std::string &seat_id, const std::string &day, const std::string &segment);
    Result<Model::MutationResult> cancel_booking(const std::string &booking_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_token;

    Result<void> ensure_login(bool force_refresh = false);
    Result<std::string> fetch_cas_token();
    Result<nlohmann::json> request_json(const std::string &path, const nlohmann::json &body, bool authorize = true, bool allow_retry = true);
};

} // namespace UBAANext
