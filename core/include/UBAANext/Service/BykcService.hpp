#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Bykc.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/HttpClient.hpp>
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

    Result<Model::BykcProfile> get_profile();
    Result<std::vector<Model::BykcCourse>> list_courses(int page = 1, int size = 100, bool all = false);
    Result<std::vector<Model::BykcCourse>> list_courses(const BykcCourseQuery &query);
    Result<std::vector<Model::BykcChosenCourse>> list_chosen_courses();
    Result<std::vector<Model::BykcStat>> list_stats();
    Result<Model::BykcCourseDetail> course_detail(const std::string &course_id);

    Result<std::vector<Model::FeatureRecord>> profile();
    Result<std::vector<Model::FeatureRecord>> courses(int page = 1, int size = 100, bool all = false);
    Result<std::vector<Model::FeatureRecord>> courses(const BykcCourseQuery &query);
    Result<std::vector<Model::FeatureRecord>> chosen();
    Result<std::vector<Model::FeatureRecord>> stats();
    Result<Model::FeatureRecord> show_course(const std::string &course_id);
    Result<Model::MutationResult> select_course(const std::string &course_id);
    Result<Model::MutationResult> unselect_course(const std::string &course_id);
    Result<Model::MutationResult> sign_course(const std::string &course_id, int sign_type);

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    std::string m_token;

    Result<void> ensure_login(bool force_refresh = false);
    Result<std::string> call_api_raw(const std::string &api_name, const nlohmann::json &payload, bool allow_retry = true);
    Result<nlohmann::json> call_api_data(const std::string &api_name, const nlohmann::json &payload, const std::string &fallback_message);
};

} // namespace UBAANext
