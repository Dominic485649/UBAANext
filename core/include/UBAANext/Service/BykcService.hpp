#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Model/Bykc.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace UBAANext {

struct BykcCourseQuery {
    int page = 1;
    int size = 100;
    bool all = false;
    std::string status;
    std::string category;
    std::string sub_category;
    std::string campus;
    std::string keyword;
};

class BykcService {
public:
    BykcService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);
    BykcService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto);

    /** ReadOnlyCandidate: fetches BYKC profile; token/session drift remains a live risk. */
    Result<Model::BykcProfile> get_profile();
    /** ReadOnlyCandidate: legacy course list overload using page/size/all. */
    Result<std::vector<Model::BykcCourse>> list_courses(int page = 1, int size = 100, bool all = false);
    /** ReadOnlyCandidate: filtered course list; unsupported filter combinations must fail explicitly. */
    Result<std::vector<Model::BykcCourse>> list_courses(const BykcCourseQuery &query);
    /** ReadOnlyCandidate: lists chosen courses; sensitive enrollment status should not be logged verbatim. */
    Result<std::vector<Model::BykcChosenCourse>> list_chosen_courses();
    /** ReadOnlyCandidate: lists BYKC stats; field drift remains possible. */
    Result<std::vector<Model::BykcStat>> list_stats();
    /** ReadOnlyCandidate: fetches a single course detail; missing ids are InvalidArgument. */
    Result<Model::BykcCourseDetail> course_detail(const std::string &course_id);

    Result<std::vector<Model::FeatureRecord>> profile();
    Result<std::vector<Model::FeatureRecord>> courses(int page = 1, int size = 100, bool all = false);
    Result<std::vector<Model::FeatureRecord>> courses(const BykcCourseQuery &query);
    Result<std::vector<Model::FeatureRecord>> chosen();
    Result<std::vector<Model::FeatureRecord>> stats();
    Result<Model::FeatureRecord> show_course(const std::string &course_id);
    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);
    /** WriteGated remote mutation: yes. Course selection requires the write gate and course id. */
    Result<Model::MutationResult> select_course(const std::string &course_id);
    /** WriteGated remote mutation: yes. Course unselection requires the write gate and course id. */
    Result<Model::MutationResult> unselect_course(const std::string &course_id);
    /** WriteGated remote mutation: yes. BYKC sign-in requires the write gate, course id, and sign type. */
    Result<Model::MutationResult> sign_course(const std::string &course_id, int sign_type);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    ICryptoProvider &m_crypto;
    WriteOperationGate m_write_gate = disabled_write_operation("bykc write");
    std::string m_token;

    Result<void> ensure_login(bool force_refresh = false);
    Result<std::string> call_api_raw_once(const std::string &api_name, const nlohmann::json &payload);
    Result<std::string> call_api_raw(const std::string &api_name, const nlohmann::json &payload, bool allow_retry = true);
    Result<nlohmann::json> call_api_data(const std::string &api_name, const nlohmann::json &payload, const std::string &fallback_message);
};

} // namespace UBAANext
