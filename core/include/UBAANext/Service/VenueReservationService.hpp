#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/VenueReservation.hpp>
#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace UBAANext {

class VenueReservationService {
public:
    VenueReservationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);
    VenueReservationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto);
    VenueReservationService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto);

    /** ReadOnlyCandidate: lists reservable venue sites; direct/proxy protocol drift remains possible. */
    Result<std::vector<Model::VenueSite>> venue_sites();
    /** ReadOnlyCandidate: lists venue purpose types used by reservation flows. */
    Result<std::vector<Model::VenuePurposeType>> purpose_types();
    /** ReadOnlyCandidate: lists spaces for a day and site; captcha/token rules are not exercised here. */
    Result<std::vector<Model::VenueSpaceInfo>> day_spaces(const std::string &date, const std::string &site_id);
    /** ReadOnlyCandidate: lists user orders; sensitive lock/order fields must not be logged verbatim. */
    Result<std::vector<Model::VenueOrder>> orders(int page = 1, int size = 20);
    /** Sensitive output: may expose order metadata and must stay behind redaction-aware output paths. */
    Result<Model::VenueOrder> order_detail(const std::string &order_id);

    Result<std::vector<Model::FeatureRecord>> list_sites();
    Result<std::vector<Model::FeatureRecord>> list_purpose_types();
    Result<std::vector<Model::FeatureRecord>> day_info(const std::string &date, const std::string &site_id);
    Result<std::vector<Model::FeatureRecord>> list_orders(int page = 1, int size = 20);
    Result<Model::FeatureRecord> show_order(const std::string &order_id);
    /** Sensitive output: lock code visibility must remain explicitly requested and redaction-aware. */
    Result<Model::FeatureRecord> lock_code();
    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);
    /**
     * WriteGated remote mutation: yes. Venue reservation consumes real capacity and requires the write gate;
     * captcha/token inputs are sensitive and must not be logged.
     */
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
    /** WriteGated remote mutation: yes. Cancels a real venue order and requires the write gate. */
    Result<Model::MutationResult> cancel_order(const std::string &order_id);

private:
    IHttpClient &m_http_client;
    ICookieStore *m_cookie_store = nullptr;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    ICryptoProvider &m_crypto;
    WriteOperationGate m_write_gate = disabled_write_operation("cgyy write");
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
