#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/LibrarySeat.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {

class LibrarySeatService {
public:
    LibrarySeatService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);
    LibrarySeatService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto);

    /** ReadOnlyCandidate: lists libraries for a day; live field drift remains possible. */
    Result<std::vector<Model::LibraryInfo>> libraries(const std::string &day);
    /** ReadOnlyCandidate: lists library areas; token/session refresh may be required. */
    Result<std::vector<Model::LibraryArea>> areas(const std::string &library_id, const std::string &day, const std::string &storey_id = "");
    /** ReadOnlyCandidate: area detail can expose capacity/seat metadata; missing ids are InvalidArgument. */
    Result<Model::LibraryArea> area_detail(const std::string &area_id);
    /** ReadOnlyCandidate: lists seats for a time range; availability is volatile and must not be cached as truth. */
    Result<std::vector<Model::LibrarySeat>> seats(const std::string &area_id, const std::string &day, const std::string &start_time = "08:00", const std::string &end_time = "22:00");
    /** Sensitive output: reservations expose booking state and must stay behind redaction-aware output paths. */
    Result<std::vector<Model::LibraryReservation>> reservations(int page = 1, int limit = 20);

    Result<std::vector<Model::FeatureRecord>> list_libraries(const std::string &day);
    Result<std::vector<Model::FeatureRecord>> list_areas(const std::string &library_id, const std::string &day, const std::string &storey_id = "");
    Result<Model::FeatureRecord> show_area(const std::string &area_id);
    Result<std::vector<Model::FeatureRecord>> list_seats(const std::string &area_id, const std::string &day, const std::string &start_time = "08:00", const std::string &end_time = "22:00");
    Result<std::vector<Model::FeatureRecord>> list_reservations(int page = 1, int limit = 20);
    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);
    /** WriteGated remote mutation: yes. Books a real seat and requires the write gate. */
    Result<Model::MutationResult> reserve_seat(const std::string &seat_id,
                                               const std::string &day,
                                               const std::string &segment,
                                               const std::string &start_time = "",
                                               const std::string &end_time = "");
    /** WriteGated remote mutation: yes. Cancels a real library booking and requires the write gate. */
    Result<Model::MutationResult> cancel_booking(const std::string &booking_id);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    ICryptoProvider &m_crypto;
    WriteOperationGate m_write_gate = disabled_write_operation("libbook write");
    std::string m_token;

    Result<void> ensure_login(bool force_refresh = false);
    Result<std::string> fetch_cas_token();
    Result<nlohmann::json> request_json(const std::string &path, const nlohmann::json &body, bool authorize = true, bool allow_retry = true);
};

} // namespace UBAANext
