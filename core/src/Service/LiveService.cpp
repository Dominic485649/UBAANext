#include <UBAANext/Service/LiveService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/LiveParser.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>

#include <array>
#include <map>
#include <regex>
#include <utility>

namespace UBAANext {
namespace {

constexpr const char *kLiveWeekScheduleUrl = "https://yjapi.msa.buaa.edu.cn/courseapi/v2/schedule/get-week-schedules";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

bool valid_date(const std::string &date) {
    static const std::regex pattern(R"(^\d{4}-\d{2}-\d{2}$)");
    return std::regex_match(date, pattern);
}

std::string url_encode_component(const std::string &value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(value.size());
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(static_cast<char>(c));
        } else {
            encoded.push_back('%');
            encoded.push_back(hex[c >> 4]);
            encoded.push_back(hex[c & 0x0F]);
        }
    }
    return encoded;
}

std::string append_query(const std::string &url, const std::map<std::string, std::string> &query) {
    std::string result = url;
    result.push_back(url.find('?') == std::string::npos ? '?' : '&');
    bool first = true;
    for (const auto &[key, value] : query) {
        if (!first) result.push_back('&');
        first = false;
        result += url_encode_component(key);
        result.push_back('=');
        result += url_encode_component(value);
    }
    return result;
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    if (json[key].is_boolean()) return json[key].get<bool>() ? "true" : "false";
    return {};
}

bool json_success(const nlohmann::json &root) {
    if (root.contains("success") && root["success"].is_boolean() && !root["success"].get<bool>()) return false;
    if (root.contains("code")) {
        auto code = json_string(root, "code");
        return code == "0" || code == "200" || code == "10000";
    }
    if (root.contains("result") && root["result"].is_object() && root["result"].contains("code")) {
        auto code = json_string(root["result"], "code");
        return code == "0" || code == "200" || code == "10000";
    }
    return true;
}

std::string json_message(const nlohmann::json &root, const std::string &fallback) {
    auto message = json_string(root, "message");
    if (message.empty()) message = json_string(root, "msg");
    if (message.empty() && root.contains("result") && root["result"].is_object()) message = json_string(root["result"], "msg");
    return message.empty() ? fallback : Security::redact_sensitive_text(message);
}

nlohmann::json schedule_list_from_envelope(const nlohmann::json &root) {
    if (root.contains("result") && root["result"].is_object()) {
        const auto &result = root["result"];
        if (result.contains("list")) return result["list"];
    }
    if (root.contains("list")) return root["list"];
    if (root.contains("data") && root["data"].is_object() && root["data"].contains("list")) return root["data"]["list"];
    return nlohmann::json::array();
}

Model::FeatureRecord schedule_to_record(const Model::LiveSchedule &schedule, int day_index) {
    static constexpr std::array<const char *, 7> day_names{"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
    Model::FeatureRecord record;
    record.id = schedule.live_id.empty() ? schedule.course_id : schedule.live_id;
    record.title = schedule.name;
    record.status = schedule.raw_status.empty() ? "scheduled" : schedule.raw_status;
    record.fields = {
        {"courseId", schedule.course_id},
        {"liveId", schedule.live_id},
        {"teacher", schedule.teacher},
        {"day", day_index >= 0 && day_index < static_cast<int>(day_names.size()) ? day_names[static_cast<std::size_t>(day_index)] : std::to_string(day_index + 1)},
    };
    return record;
}

} // namespace

LiveService::LiveService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<Model::LiveWeekSchedule> LiveService::get_week_schedule(const LiveWeekQuery &query) {
    (void)m_cache;
    if (!valid_date(query.start_date) || !valid_date(query.end_date)) {
        return make_error(ErrorCode::InvalidArgument, "live week 需要 --start-date/--end-date，格式为 yyyy-MM-dd");
    }

    const auto url = append_query(kLiveWeekScheduleUrl, {{"end_at", query.end_date}, {"start_at", query.start_date}});
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["User-Agent"] = kUserAgent;
    request.headers["Referer"] = resolve_for_mode("https://classroom.msa.buaa.edu.cn/", m_mode);

    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, response.error().message);
    if (Protocol::is_session_expired_response(*response, {}, false)) return make_error(ErrorCode::SessionExpired, "课堂直播会话已过期，请重新登录");
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "课堂直播周课表请求返回: " + std::to_string(response->status_code));

    auto root = nlohmann::json::parse(response->body, nullptr, false);
    if (root.is_discarded()) return make_error(ErrorCode::ParseError, "解析课堂直播周课表 JSON 失败");
    if (!json_success(root)) return make_error(ErrorCode::NetworkError, json_message(root, "课堂直播周课表加载失败"));

    Model::LiveWeekSchedule result;
    result.start_date = query.start_date;
    result.end_date = query.end_date;
    result.days = Parser::parse_live_week_schedule_days(schedule_list_from_envelope(root));
    while (result.days.size() < 7) result.days.emplace_back();
    if (result.days.size() > 7) result.days.resize(7);
    return result;
}

Result<std::vector<Model::FeatureRecord>> LiveService::week_schedule_records(const LiveWeekQuery &query) {
    auto week = get_week_schedule(query);
    if (!week) return make_error(week.error().code, week.error().message);
    std::vector<Model::FeatureRecord> records;
    for (std::size_t day = 0; day < week->days.size(); ++day) {
        for (const auto &schedule : week->days[day]) records.push_back(schedule_to_record(schedule, static_cast<int>(day)));
    }
    return records;
}

} // namespace UBAANext
