#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Live.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>

#include <string>
#include <vector>

namespace UBAANext {

struct LiveWeekQuery {
    std::string start_date;
    std::string end_date;
};

class LiveService {
public:
    LiveService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode);

    /** ReadOnlyCandidate: fetches classroom live schedules for a date range, usually a Monday-Sunday week. */
    Result<Model::LiveWeekSchedule> get_week_schedule(const LiveWeekQuery &query);
    /** ReadOnlyCandidate: FeatureRecord projection for CLI/Todo consumers. */
    Result<std::vector<Model::FeatureRecord>> week_schedule_records(const LiveWeekQuery &query);
    /** ReadOnlyCandidate: searches classroom resources for a day, optionally filtered by playback/live status. */
    Result<std::vector<Model::LiveResource>> resources(const Model::LiveResourceQuery &query);
    /** ReadOnlyCandidate: fetches one classroom resource detail, then enriches it from livingroom HTML when available. */
    Result<Model::LiveResourceDetail> detail(const std::string &course_id, const std::string &sub_id, const std::string &date = {});
    /** ReadOnlyCandidate: fetches PPT slide timeline for one GUID. */
    Result<std::vector<Model::LivePptSlide>> ppt_slides(const std::string &course_id, const std::string &sub_id, const std::string &resource_guid);
    /** ReadOnlyCandidate: builds a PPTX byte payload after downloading slide images for the first successful GUID. */
    Result<Model::LiveDownloadResult> prepare_download(const Model::LiveResourceDetail &detail,
                                                       const std::vector<std::string> &guid_candidates,
                                                       bool include_ppt,
                                                       bool include_video);
    /** ReadOnlyCandidate: downloads one URL as bytes using classroom headers. */
    Result<Model::LiveBinaryResource> download_binary(const std::string &url, const std::string &name = {});

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
