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

private:
    IHttpClient &m_http_client;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
};

} // namespace UBAANext
