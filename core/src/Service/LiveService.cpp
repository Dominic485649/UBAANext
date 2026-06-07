#include <UBAANext/Service/LiveService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/LiveParser.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <optional>
#include <regex>
#include <utility>

namespace UBAANext {
namespace {

constexpr const char *kLiveWeekScheduleUrl = "https://yjapi.msa.buaa.edu.cn/courseapi/v2/schedule/get-week-schedules";
constexpr const char *kLiveCourseDetailUrl = "https://yjapi.msa.buaa.edu.cn/courseapi/v2/course-live/search-live-course-list";
constexpr const char *kLivePptTimelineUrl = "https://classroom.msa.buaa.edu.cn/pptnote/v1/schedule/search-ppt";
constexpr const char *kLiveLivingroomUrl = "https://classroom.msa.buaa.edu.cn/livingroom";

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

bool classroom_success(const nlohmann::json &root) {
    if (root.contains("code")) {
        auto code = json_string(root, "code");
        return code == "0" || code == "200" || code == "10000";
    }
    if (root.contains("success") && root["success"].is_boolean()) return root["success"].get<bool>();
    return true;
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

std::string normalize_status_filter(std::string status) {
    std::transform(status.begin(), status.end(), status.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (status.empty()) return "all";
    return status;
}

bool status_matches(const Model::LiveResource &resource, const std::string &status) {
    if (status == "all" || status.empty()) return true;
    if (status == "live") return resource.status_label == "直播中";
    if (status == "playback") return resource.status_label == "回放";
    if (status == "generating") return resource.status_label == "回放生成中";
    return resource.status_label == status;
}

void apply_classroom_headers(HttpRequest &request, ConnectionMode mode, const std::string &accept = "application/json, text/plain, */*") {
    request.headers["Accept"] = accept;
    request.headers["User-Agent"] = kUserAgent;
    request.headers["Referer"] = resolve_for_mode("https://classroom.msa.buaa.edu.cn/", mode);
    request.headers["Origin"] = resolve_for_mode("https://classroom.msa.buaa.edu.cn", mode);
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    for (const auto &[key, value] : response.headers) {
        if (key.size() != name.size()) continue;
        bool match = true;
        for (std::size_t i = 0; i < key.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(key[i])) != std::tolower(static_cast<unsigned char>(name[i]))) {
                match = false;
                break;
            }
        }
        if (match) return value;
    }
    return {};
}

nlohmann::json extract_detail_candidates(const nlohmann::json &root) {
    if (root.is_array()) return root;
    if (!root.is_object()) return nlohmann::json::array();
    if (root.contains("list")) return root["list"];
    if (root.contains("result") && root["result"].is_object() && root["result"].contains("list")) return root["result"]["list"];
    if (root.contains("data") && root["data"].is_object() && root["data"].contains("list")) return root["data"]["list"];
    return nlohmann::json::array();
}

bool candidate_matches(const nlohmann::json &item, const std::string &course_id, const std::string &sub_id) {
    if (!item.is_object()) return false;
    auto item_course = json_string(item, "course_id");
    if (item_course.empty()) item_course = json_string(item, "courseId");
    if (item_course.empty()) item_course = json_string(item, "c.course_id");
    if (item_course.empty()) item_course = json_string(item, "id");
    auto item_sub = json_string(item, "sub_id");
    if (item_sub.empty()) item_sub = json_string(item, "subId");
    if (item_sub.empty()) item_sub = json_string(item, "sub.id");
    return item_course == course_id && item_sub == sub_id;
}

std::optional<nlohmann::json> find_detail_item(const nlohmann::json &candidates, const std::string &course_id, const std::string &sub_id) {
    if (candidates.is_object()) {
        if (candidate_matches(candidates, course_id, sub_id)) return candidates;
        if (candidates.contains("list")) return find_detail_item(candidates["list"], course_id, sub_id);
        return std::nullopt;
    }
    if (!candidates.is_array()) return std::nullopt;
    for (const auto &candidate : candidates) {
        if (!candidate.is_object()) continue;
        if (candidate_matches(candidate, course_id, sub_id)) return candidate;
        if (candidate.contains("list")) {
            auto nested = find_detail_item(candidate["list"], course_id, sub_id);
            if (nested) return nested;
        }
    }
    return std::nullopt;
}

void push_unique(std::vector<std::string> &values, const std::string &value) {
    if (value.empty()) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
}

void merge_livingroom(Model::LiveResourceDetail &detail, const Model::LiveResourceDetail &livingroom) {
    for (const auto &guid : livingroom.ppt_guids) push_unique(detail.ppt_guids, guid);
    if (detail.ppt_resource_guid.empty() && !livingroom.ppt_resource_guid.empty()) detail.ppt_resource_guid = livingroom.ppt_resource_guid;
    if (detail.primary_video_url.empty()) {
        detail.primary_video_url = livingroom.primary_video_url;
        detail.primary_video_hls = livingroom.primary_video_hls;
    }
    if (detail.live_url.empty()) detail.live_url = livingroom.live_url;
    if (detail.playback_url.empty()) detail.playback_url = livingroom.playback_url;
    for (const auto &source : livingroom.video_sources) {
        auto duplicate = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [&](const Model::LiveVideoSource &existing) {
            return existing.url == source.url;
        });
        if (duplicate == detail.video_sources.end()) detail.video_sources.push_back(source);
    }
    detail.has_video = detail.has_video || livingroom.has_video;
}

bool looks_like_image(const std::vector<std::uint8_t> &bytes) {
    if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) return true;
    if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') return true;
    if (bytes.size() >= 4 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8') return true;
    return false;
}

std::string file_name_from_url(const std::string &url, const std::string &fallback) {
    auto trimmed = url.substr(0, url.find_first_of("?#"));
    auto slash = trimmed.find_last_of('/');
    auto name = slash == std::string::npos ? trimmed : trimmed.substr(slash + 1);
    return name.empty() ? fallback : name;
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

Result<std::vector<Model::LiveResource>> LiveService::resources(const Model::LiveResourceQuery &query) {
    (void)m_cache;
    if (!valid_date(query.date)) {
        return make_error(ErrorCode::InvalidArgument, "live resources 需要 --date <yyyy-MM-dd>");
    }
    auto status = normalize_status_filter(query.status);
    if (status != "all" && status != "live" && status != "playback" && status != "generating") {
        return make_error(ErrorCode::InvalidArgument, "live resources --status 只支持 live、playback、generating 或 all");
    }

    const int page = query.page <= 0 ? 1 : query.page;
    const int size = query.size <= 0 ? 100 : std::min(query.size, 200);
    const auto url = append_query(kLiveCourseDetailUrl,
                                  {{"all", "1"},
                                   {"page", std::to_string(page)},
                                   {"per_page", std::to_string(size)},
                                   {"search_time", query.date},
                                   {"show_all", "1"},
                                   {"show_delete", "2"},
                                   {"with_room_data", "1"},
                                   {"with_sub_data", "1"}});

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    apply_classroom_headers(request, m_mode);

    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, response.error().message);
    if (Protocol::is_session_expired_response(*response, {}, false)) return make_error(ErrorCode::SessionExpired, "课堂资源会话已过期，请重新登录");
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "课堂资源请求返回: " + std::to_string(response->status_code));

    auto root = nlohmann::json::parse(response->body, nullptr, false);
    if (root.is_discarded()) return make_error(ErrorCode::ParseError, "解析课堂资源 JSON 失败");
    if (!classroom_success(root)) return make_error(ErrorCode::NetworkError, json_message(root, "课堂资源加载失败"));

    auto parsed = Parser::parse_live_resources(root, "live");
    std::vector<Model::LiveResource> filtered;
    for (auto &resource : parsed) {
        if (status_matches(resource, status)) filtered.push_back(std::move(resource));
    }
    return filtered;
}

Result<Model::LiveResourceDetail> LiveService::detail(const std::string &course_id, const std::string &sub_id, const std::string &date) {
    if (course_id.empty() || sub_id.empty()) {
        return make_error(ErrorCode::InvalidArgument, "live detail 需要 --course-id 和 --sub-id");
    }

    const auto url = append_query(kLiveCourseDetailUrl,
                                  {{"all", "1"},
                                   {"course_id", course_id},
                                   {"show_all", "1"},
                                   {"show_delete", "2"},
                                   {"sub_id", sub_id},
                                   {"with_room_data", "1"},
                                   {"with_sub_data", "1"}});

    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    apply_classroom_headers(request, m_mode);

    std::optional<Model::LiveResourceDetail> parsed_detail;
    auto response = m_http_client.send(request);
    if (response && response->status_code == 200 && !Protocol::is_session_expired_response(*response, {}, false)) {
        auto root = nlohmann::json::parse(response->body, nullptr, false);
        if (!root.is_discarded() && classroom_success(root)) {
            auto item = find_detail_item(extract_detail_candidates(root), course_id, sub_id);
            if (item) parsed_detail = Parser::parse_live_resource_detail(*item);
        }
    } else if (!response) {
        return make_error(response.error().code, response.error().message);
    } else if (Protocol::is_session_expired_response(*response, {}, false)) {
        return make_error(ErrorCode::SessionExpired, "课堂资源会话已过期，请重新登录");
    }

    if (!parsed_detail && !date.empty()) {
        auto list = resources({date, "all", false, 1, 200});
        if (!list) return make_error(list.error().code, list.error().message);
        auto found = std::find_if(list->begin(), list->end(), [&](const Model::LiveResource &resource) {
            return resource.course_id == course_id && resource.sub_id == sub_id;
        });
        if (found != list->end()) {
            Model::LiveResourceDetail fallback;
            static_cast<Model::LiveResource &>(fallback) = *found;
            parsed_detail = fallback;
        }
    }

    if (!parsed_detail) return make_error(ErrorCode::ParseError, "未找到指定课堂资源详情");
    parsed_detail->course_id = parsed_detail->course_id.empty() ? course_id : parsed_detail->course_id;
    parsed_detail->sub_id = parsed_detail->sub_id.empty() ? sub_id : parsed_detail->sub_id;

    const auto livingroom_url = append_query(kLiveLivingroomUrl, {{"course_id", course_id}, {"sub_id", sub_id}});
    HttpRequest livingroom_request;
    livingroom_request.method = HttpMethod::Get;
    livingroom_request.url = resolve_for_mode(livingroom_url, m_mode);
    apply_classroom_headers(livingroom_request, m_mode, "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    auto livingroom_response = m_http_client.send(livingroom_request);
    if (livingroom_response && livingroom_response->status_code == 200) {
        merge_livingroom(*parsed_detail, Parser::parse_live_livingroom_html(livingroom_response->body));
    }

    if (parsed_detail->ppt_resource_guid.empty() && !parsed_detail->ppt_guids.empty()) parsed_detail->ppt_resource_guid = parsed_detail->ppt_guids.front();
    return *parsed_detail;
}

Result<std::vector<Model::LivePptSlide>> LiveService::ppt_slides(const std::string &course_id, const std::string &sub_id, const std::string &resource_guid) {
    if (course_id.empty() || sub_id.empty() || resource_guid.empty()) {
        return make_error(ErrorCode::InvalidArgument, "live PPT 时间轴需要 course_id、sub_id 和 resource_guid");
    }
    std::vector<Model::LivePptSlide> slides;
    for (int page = 1; page <= 10; ++page) {
        const auto url = append_query(kLivePptTimelineUrl,
                                      {{"course_id", course_id},
                                       {"page", std::to_string(page)},
                                       {"per_page", "100"},
                                       {"resource_guid", resource_guid},
                                       {"sub_id", sub_id}});
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(url, m_mode);
        apply_classroom_headers(request, m_mode);
        auto response = m_http_client.send(request);
        if (!response) return make_error(response.error().code, response.error().message);
        if (Protocol::is_session_expired_response(*response, {}, false)) return make_error(ErrorCode::SessionExpired, "课堂 PPT 会话已过期，请重新登录");
        if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "课堂 PPT 时间轴请求返回: " + std::to_string(response->status_code));
        auto root = nlohmann::json::parse(response->body, nullptr, false);
        if (root.is_discarded()) return make_error(ErrorCode::ParseError, "解析课堂 PPT 时间轴 JSON 失败");
        if (!classroom_success(root)) break;
        auto batch = Parser::parse_live_ppt_slides(root);
        if (batch.empty()) break;
        slides.insert(slides.end(), batch.begin(), batch.end());
        if (batch.size() < 100) break;
    }
    std::sort(slides.begin(), slides.end(), [](const Model::LivePptSlide &lhs, const Model::LivePptSlide &rhs) {
        if (lhs.time_sec == rhs.time_sec) return lhs.index < rhs.index;
        return lhs.time_sec < rhs.time_sec;
    });
    for (std::size_t i = 0; i < slides.size(); ++i) slides[i].index = static_cast<int>(i);
    return slides;
}

Result<Model::LiveBinaryResource> LiveService::download_binary(const std::string &url, const std::string &name) {
    if (url.empty()) return make_error(ErrorCode::InvalidArgument, "下载 URL 不能为空");
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    apply_classroom_headers(request, m_mode, "*/*");
    auto response = m_http_client.send(request);
    if (!response) return make_error(response.error().code, response.error().message);
    if (Protocol::is_session_expired_response(*response, {}, false)) return make_error(ErrorCode::SessionExpired, "课堂资源下载会话已过期，请重新登录");
    if (response->status_code != 200 && response->status_code != 206) {
        return make_error(ErrorCode::NetworkError, "课堂资源下载返回: " + std::to_string(response->status_code));
    }
    Model::LiveBinaryResource binary;
    binary.name = name.empty() ? file_name_from_url(url, "download.bin") : name;
    binary.content_type = header_value(*response, "content-type");
    binary.bytes.assign(response->body.begin(), response->body.end());
    return binary;
}

Result<Model::LiveDownloadResult> LiveService::prepare_download(const Model::LiveResourceDetail &detail,
                                                                const std::vector<std::string> &guid_candidates,
                                                                bool include_ppt,
                                                                bool include_video) {
    Model::LiveDownloadResult result;
    result.course_id = detail.course_id;
    result.sub_id = detail.sub_id;
    result.title = detail.title;

    if (include_ppt) {
        std::vector<std::string> guids;
        for (const auto &guid : guid_candidates) push_unique(guids, guid);
        for (const auto &guid : detail.ppt_guids) push_unique(guids, guid);
        push_unique(guids, detail.ppt_resource_guid);
        push_unique(guids, detail.sub_resource_guid);

        for (const auto &guid : guids) {
            auto slides = ppt_slides(detail.course_id, detail.sub_id, guid);
            if (!slides) {
                result.failed_images.push_back("guid:" + guid + ":" + slides.error().message);
                continue;
            }
            if (slides->empty()) continue;
            result.used_guid = guid;
            result.slides = *slides;
            break;
        }

        if (!result.slides.empty()) {
            for (const auto &slide : result.slides) {
                auto image = download_binary(slide.image_url, "slide-" + std::to_string(slide.index + 1));
                if (!image) {
                    result.failed_images.push_back(slide.image_url);
                    continue;
                }
                if (!looks_like_image(image->bytes)) {
                    result.failed_images.push_back(slide.image_url);
                    continue;
                }
                result.images.push_back(std::move(*image));
            }
            if (!result.images.empty()) {
                result.pptx_bytes = Parser::build_live_pptx(result.images);
                result.status = result.failed_images.empty() ? "completed" : "partial";
            } else {
                result.status = "error";
                result.message = "所有 PPT 图片下载失败";
            }
        } else {
            result.status = "partial";
            result.message = "没有可下载的 PPT 时间轴数据";
        }
    }

    if (include_video) {
        result.video_url = detail.primary_video_url.empty() ? detail.playback_url : detail.primary_video_url;
        result.video_hls = detail.primary_video_url.empty() ? detail.playback_hls : detail.primary_video_hls;
        if (!result.video_url.empty() && !result.video_hls) {
            auto video = download_binary(result.video_url, "video.mp4");
            if (video) {
                result.video_bytes = std::move(video->bytes);
                if (result.status == "empty" || result.status == "partial") result.status = result.status == "partial" ? "partial" : "completed";
            } else {
                result.status = result.status == "completed" ? "partial" : "error";
                if (!result.message.empty()) result.message += "; ";
                result.message += "视频下载失败: " + video.error().message;
            }
        } else if (!result.video_url.empty() && result.video_hls) {
            if (result.status == "empty") result.status = "partial";
            if (!result.message.empty()) result.message += "; ";
            result.message += "视频为 HLS，需由 CLI 调用 ffmpeg 或写入 m3u8 sidecar";
        } else {
            if (result.status == "empty") result.status = "partial";
            if (!result.message.empty()) result.message += "; ";
            result.message += "没有可下载的视频 URL";
        }
    }

    if (result.status == "empty") result.status = "completed";
    return result;
}

} // namespace UBAANext
