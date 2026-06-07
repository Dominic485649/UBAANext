#include <UBAANext/Parser/LiveParser.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    if (json[key].is_boolean()) return json[key].get<bool>() ? "true" : "false";
    return {};
}

int json_int(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return 0;
    if (json[key].is_number_integer()) return json[key].get<int>();
    if (json[key].is_number_unsigned()) return static_cast<int>(json[key].get<unsigned int>());
    if (json[key].is_number_float()) return static_cast<int>(json[key].get<double>());
    if (json[key].is_string()) {
        try {
            return std::stoi(json[key].get<std::string>());
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

nlohmann::json normalize_day_courses(const nlohmann::json &day) {
    if (day.is_array()) return day;
    if (day.is_object()) {
        if (day.contains("course") && day["course"].is_array()) return day["course"];
        if (day.contains("courses") && day["courses"].is_array()) return day["courses"];
        if (day.contains("list") && day["list"].is_array()) return day["list"];
    }
    return nlohmann::json::array();
}

std::string status_label(int sub_status, const std::string &fallback) {
    switch (sub_status) {
    case 1: return "直播中";
    case 2: return "未开始";
    case 3:
    case 5: return "回放生成中";
    case 4: return "已结束";
    case 6:
    case 7: return "回放";
    default: return fallback.empty() ? "未知" : fallback;
    }
}

bool is_hls_url(const std::string &url) {
    return url.find(".m3u8") != std::string::npos;
}

bool is_video_url(const std::string &url) {
    return url.find(".m3u8") != std::string::npos || url.find(".mp4") != std::string::npos;
}

std::string normalize_url_text(std::string text) {
    auto replace_all = [](std::string &value, const std::string &from, const std::string &to) {
        std::size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    replace_all(text, "\\/", "/");
    replace_all(text, "&amp;", "&");
    replace_all(text, "\\u0026", "&");
    replace_all(text, "\\u003d", "=");
    while (!text.empty() && (text.back() == ',' || text.back() == ';' || text.back() == ')' || text.back() == ']' || text.back() == '}')) {
        text.pop_back();
    }
    return text;
}

void push_unique(std::vector<std::string> &values, std::string value) {
    if (value.empty()) return;
    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(std::move(value));
}

void push_video_source(std::vector<Model::LiveVideoSource> &sources, std::string kind, std::string url) {
    url = normalize_url_text(std::move(url));
    if (url.empty() || !is_video_url(url)) return;
    const auto duplicate = std::find_if(sources.begin(), sources.end(), [&](const Model::LiveVideoSource &source) {
        return source.url == url;
    });
    if (duplicate != sources.end()) return;
    Model::LiveVideoSource source;
    source.kind = std::move(kind);
    source.hls = is_hls_url(url);
    source.url = std::move(url);
    sources.push_back(std::move(source));
}

nlohmann::json parse_json_maybe(const nlohmann::json &value) {
    if (value.is_object() || value.is_array()) return value;
    if (!value.is_string()) return nlohmann::json::object();
    auto parsed = nlohmann::json::parse(value.get<std::string>(), nullptr, false);
    return parsed.is_discarded() ? nlohmann::json::object() : parsed;
}

void extract_video_urls_recursive(const nlohmann::json &value, Model::LiveResourceDetail &detail, int depth = 0) {
    if (depth > 8) return;
    if (value.is_string()) {
        auto text = normalize_url_text(value.get<std::string>());
        if (is_video_url(text)) push_video_source(detail.video_sources, "json", std::move(text));
        return;
    }
    if (value.is_array()) {
        for (const auto &item : value) extract_video_urls_recursive(item, detail, depth + 1);
        return;
    }
    if (!value.is_object()) return;
    for (const auto *key : {"stream_url", "m3u8_url", "playback_url", "video_url", "play_url", "preview_url", "contents", "src", "url"}) {
        auto url = json_string(value, key);
        if (!url.empty()) push_video_source(detail.video_sources, std::string("json.") + key, std::move(url));
    }
    for (const auto *key : {"resource_guid", "guid", "sub_resource_guid"}) {
        auto guid = json_string(value, key);
        if (guid.size() == 32) push_unique(detail.ppt_guids, std::move(guid));
    }
    for (const auto &[_, child] : value.items()) extract_video_urls_recursive(child, detail, depth + 1);
}

std::string first_of(const nlohmann::json &item, const std::vector<const char *> &keys) {
    for (const auto *key : keys) {
        auto value = json_string(item, key);
        if (!value.empty()) return value;
    }
    return {};
}

Model::LiveResource normalize_live_resource(const nlohmann::json &item, const std::string &slot_label, const std::string &slot_time, const std::string &source) {
    Model::LiveResource resource;
    resource.course_id = first_of(item, {"course_id", "courseId", "id", "c.course_id"});
    resource.sub_id = first_of(item, {"sub_id", "subId"});
    resource.title = first_of(item, {"title", "course_title", "courseTitle", "name"});
    resource.course_code = first_of(item, {"course_code", "courseCode"});
    resource.teacher = first_of(item, {"lecturer_name", "realname", "teacher_name", "teacherName", "teacher"});
    resource.room = first_of(item, {"room_name", "roomName", "classroom", "place"});
    resource.sub_title = first_of(item, {"sub_title", "subject_title"});
    const auto sub_status = json_int(item, "sub_status");
    const auto live_status = json_int(item, "live_status");
    const auto api_label = first_of(item, {"status_label", "sub_type", "status"});
    resource.status_label = status_label(sub_status, api_label);
    if (sub_status != 0 || live_status != 0 || !api_label.empty()) {
        resource.raw_status = api_label + " (sub=" + std::to_string(sub_status) + ", live=" + std::to_string(live_status) + ")";
    }
    resource.term_name = json_string(item, "term_name");
    resource.course_time = first_of(item, {"course_time", "create_at", "course_begin"});
    resource.time_slot = slot_label;
    resource.time_range = slot_time;
    resource.thumb_url = first_of(item, {"extract_thumb", "thumb"});
    resource.source = source;
    return resource;
}

nlohmann::json list_from_resource_envelope(const nlohmann::json &root) {
    if (root.is_array()) return root;
    if (!root.is_object()) return nlohmann::json::array();
    if (root.contains("list")) return root["list"];
    if (root.contains("result") && root["result"].is_object() && root["result"].contains("list")) return root["result"]["list"];
    if (root.contains("data") && root["data"].is_object() && root["data"].contains("list")) return root["data"]["list"];
    if (root.contains("total") && root["total"].is_object() && root["total"].contains("list")) return root["total"]["list"];
    return nlohmann::json::array();
}

void append_flattened_resources(const nlohmann::json &list, std::vector<Model::LiveResource> &resources, const std::string &source) {
    if (list.is_object()) {
        if (list.contains("course_id") || list.contains("courseId") || list.contains("sub_id") || list.contains("subId") || list.contains("c.course_id")) {
            resources.push_back(normalize_live_resource(list, "", "", source));
            return;
        }
        if (list.contains("list")) append_flattened_resources(list["list"], resources, source);
        return;
    }
    if (!list.is_array()) return;
    for (const auto &slot : list) {
        if (!slot.is_object()) continue;
        if (slot.contains("course_id") || slot.contains("courseId") || slot.contains("sub_id") || slot.contains("subId") || slot.contains("c.course_id")) {
            resources.push_back(normalize_live_resource(slot, "", "", source));
            continue;
        }
        const auto slot_label = first_of(slot, {"name", "label"});
        auto begin = json_string(slot, "class_begin_time");
        auto end = json_string(slot, "class_end_time");
        const auto slot_time = (!begin.empty() || !end.empty()) ? begin + "-" + end : std::string{};
        if (slot.contains("list")) {
            const auto &items = slot["list"];
            if (items.is_array()) {
                for (const auto &item : items) {
                    if (item.is_object()) resources.push_back(normalize_live_resource(item, slot_label, slot_time, source));
                }
            }
        }
    }
}

std::string xml_escape(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string image_extension(const Model::LiveBinaryResource &image, int index) {
    auto lower = image.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const auto &ext : {".jpg", ".jpeg", ".png", ".gif"}) {
        if (lower.size() >= std::strlen(ext) && lower.compare(lower.size() - std::strlen(ext), std::strlen(ext), ext) == 0) {
            return std::string(ext) == ".jpeg" ? ".jpg" : std::string(ext);
        }
    }
    if (image.content_type.find("png") != std::string::npos) return ".png";
    if (image.content_type.find("gif") != std::string::npos) return ".gif";
    (void)index;
    return ".jpg";
}

std::uint32_t crc32_bytes(const std::vector<std::uint8_t> &bytes) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::uint8_t byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & static_cast<std::uint32_t>(-static_cast<int>(crc & 1u)));
        }
    }
    return ~crc;
}

std::vector<std::uint8_t> bytes_from_string(const std::string &text) {
    return {text.begin(), text.end()};
}

void append_u16(std::vector<std::uint8_t> &out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

void append_text(std::vector<std::uint8_t> &out, const std::string &text) {
    out.insert(out.end(), text.begin(), text.end());
}

struct ZipEntryInfo {
    std::string name;
    std::uint32_t crc = 0;
    std::uint32_t size = 0;
    std::uint32_t offset = 0;
};

class StoredZipWriter {
public:
    void add(std::string name, const std::vector<std::uint8_t> &data) {
        ZipEntryInfo info;
        info.name = std::move(name);
        info.crc = crc32_bytes(data);
        info.size = static_cast<std::uint32_t>(data.size());
        info.offset = static_cast<std::uint32_t>(m_bytes.size());

        append_u32(m_bytes, 0x04034b50u);
        append_u16(m_bytes, 20);
        append_u16(m_bytes, 0);
        append_u16(m_bytes, 0);
        append_u16(m_bytes, 0);
        append_u16(m_bytes, 0);
        append_u32(m_bytes, info.crc);
        append_u32(m_bytes, info.size);
        append_u32(m_bytes, info.size);
        append_u16(m_bytes, static_cast<std::uint16_t>(info.name.size()));
        append_u16(m_bytes, 0);
        append_text(m_bytes, info.name);
        m_bytes.insert(m_bytes.end(), data.begin(), data.end());
        m_entries.push_back(std::move(info));
    }

    std::vector<std::uint8_t> finish() {
        const auto central_offset = static_cast<std::uint32_t>(m_bytes.size());
        for (const auto &info : m_entries) {
            append_u32(m_bytes, 0x02014b50u);
            append_u16(m_bytes, 20);
            append_u16(m_bytes, 20);
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u32(m_bytes, info.crc);
            append_u32(m_bytes, info.size);
            append_u32(m_bytes, info.size);
            append_u16(m_bytes, static_cast<std::uint16_t>(info.name.size()));
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u16(m_bytes, 0);
            append_u32(m_bytes, 0);
            append_u32(m_bytes, info.offset);
            append_text(m_bytes, info.name);
        }
        const auto central_size = static_cast<std::uint32_t>(m_bytes.size()) - central_offset;
        append_u32(m_bytes, 0x06054b50u);
        append_u16(m_bytes, 0);
        append_u16(m_bytes, 0);
        append_u16(m_bytes, static_cast<std::uint16_t>(m_entries.size()));
        append_u16(m_bytes, static_cast<std::uint16_t>(m_entries.size()));
        append_u32(m_bytes, central_size);
        append_u32(m_bytes, central_offset);
        append_u16(m_bytes, 0);
        return std::move(m_bytes);
    }

private:
    std::vector<std::uint8_t> m_bytes;
    std::vector<ZipEntryInfo> m_entries;
};

} // namespace

std::vector<std::vector<Model::LiveSchedule>> parse_live_week_schedule_days(const nlohmann::json &list) {
    std::vector<std::vector<Model::LiveSchedule>> days;
    if (!list.is_array()) return days;
    days.reserve(list.size());
    for (const auto &day : list) {
        std::vector<Model::LiveSchedule> schedules;
        for (const auto &item : normalize_day_courses(day)) {
            Model::LiveSchedule schedule;
            schedule.course_id = json_string(item, "course_id");
            if (schedule.course_id.empty()) schedule.course_id = json_string(item, "courseId");
            schedule.live_id = json_string(item, "id");
            if (schedule.live_id.empty()) schedule.live_id = json_string(item, "live_id");
            schedule.name = json_string(item, "course_title");
            if (schedule.name.empty()) schedule.name = json_string(item, "courseTitle");
            if (schedule.name.empty()) schedule.name = json_string(item, "name");
            schedule.teacher = json_string(item, "teacher_name");
            if (schedule.teacher.empty()) schedule.teacher = json_string(item, "teacherName");
            if (schedule.teacher.empty()) schedule.teacher = json_string(item, "teacher");
            schedule.raw_status = json_string(item, "status");
            if (!schedule.course_id.empty() || !schedule.live_id.empty() || !schedule.name.empty()) schedules.push_back(std::move(schedule));
        }
        days.push_back(std::move(schedules));
    }
    return days;
}

std::vector<Model::LiveResource> parse_live_resources(const nlohmann::json &root, const std::string &source) {
    std::vector<Model::LiveResource> resources;
    append_flattened_resources(list_from_resource_envelope(root), resources, source);
    resources.erase(std::remove_if(resources.begin(), resources.end(), [](const Model::LiveResource &resource) {
                        return resource.course_id.empty() && resource.sub_id.empty() && resource.title.empty();
                    }),
                    resources.end());
    return resources;
}

Model::LiveResourceDetail parse_live_resource_detail(const nlohmann::json &item) {
    Model::LiveResourceDetail detail;
    static_cast<Model::LiveResource &>(detail) = normalize_live_resource(item, "", "", "live");
    detail.sub_resource_guid = json_string(item, "sub_resource_guid");
    detail.ppt_resource_guid = detail.sub_resource_guid;
    push_unique(detail.ppt_guids, detail.sub_resource_guid);

    auto sub_content = parse_json_maybe(item.contains("sub_content") ? item["sub_content"] : nlohmann::json::object());
    auto playback = sub_content.contains("save_playback") ? sub_content["save_playback"] : nlohmann::json::object();
    detail.playback_url = json_string(playback, "contents");
    detail.playback_hls = json_string(playback, "is_m3u8") == "yes" || is_hls_url(detail.playback_url);
    if (!detail.playback_url.empty()) push_video_source(detail.video_sources, "save_playback", detail.playback_url);

    for (const auto *section : {"output", "output_student", "tts"}) {
        if (sub_content.contains(section) && sub_content[section].is_object()) {
            auto url = json_string(sub_content[section], "m3u8");
            if (!url.empty() && detail.live_url.empty()) detail.live_url = url;
            push_video_source(detail.video_sources, std::string("sub_content.") + section, std::move(url));
        }
    }
    auto trans_socket_url = json_string(sub_content, "trans_socket_url");
    if (!trans_socket_url.empty() && detail.live_url.empty()) detail.live_url = trans_socket_url;
    push_video_source(detail.video_sources, "trans_socket_url", std::move(trans_socket_url));

    if (item.contains("video_list") && item["video_list"].is_array()) {
        for (const auto &video : item["video_list"]) {
            auto resource_guid = json_string(video, "resource_guid");
            auto preview_url = json_string(video, "preview_url");
            const auto type = json_string(video, "type");
            if (type == "2") {
                if (!resource_guid.empty()) detail.ppt_resource_guid = resource_guid;
                if (!preview_url.empty()) detail.ppt_video_url = preview_url;
                push_unique(detail.ppt_guids, resource_guid);
                push_video_source(detail.video_sources, "ppt_video", std::move(preview_url));
            } else if (type == "3") {
                if (detail.playback_url.empty()) detail.playback_url = preview_url;
                push_video_source(detail.video_sources, "teacher_video", std::move(preview_url));
            } else {
                push_video_source(detail.video_sources, "video_list", std::move(preview_url));
            }
        }
    }

    if (item.contains("segment_video_list") && item["segment_video_list"].is_array()) {
        for (const auto &segment : item["segment_video_list"]) {
            if (!segment.is_object()) continue;
            for (const auto *key : {"ppt_list", "teacher_list"}) {
                if (!segment.contains(key) || !segment[key].is_object()) continue;
                const auto &part = segment[key];
                auto resource_guid = json_string(part, "resource_guid");
                auto preview_url = json_string(part, "preview_url");
                push_unique(detail.ppt_guids, resource_guid);
                if (std::string(key) == "ppt_list" && !resource_guid.empty()) detail.ppt_resource_guid = resource_guid;
                push_video_source(detail.video_sources, key, std::move(preview_url));
            }
        }
    }

    extract_video_urls_recursive(item, detail);

    if (detail.primary_video_url.empty()) {
        auto live = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [](const Model::LiveVideoSource &source) { return source.hls; });
        if (live != detail.video_sources.end()) {
            detail.primary_video_url = live->url;
            detail.primary_video_hls = live->hls;
        } else if (!detail.video_sources.empty()) {
            detail.primary_video_url = detail.video_sources.front().url;
            detail.primary_video_hls = detail.video_sources.front().hls;
        }
    }
    if (detail.live_url.empty()) {
        auto live = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [](const Model::LiveVideoSource &source) { return source.hls; });
        if (live != detail.video_sources.end()) detail.live_url = live->url;
    }
    if (detail.playback_url.empty()) {
        auto replay = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [](const Model::LiveVideoSource &source) { return !source.hls; });
        if (replay != detail.video_sources.end()) {
            detail.playback_url = replay->url;
            detail.playback_hls = false;
        }
    }
    detail.has_video = !detail.primary_video_url.empty() || !detail.video_sources.empty();
    if (!detail.ppt_resource_guid.empty()) push_unique(detail.ppt_guids, detail.ppt_resource_guid);
    return detail;
}

Model::LiveResourceDetail parse_live_livingroom_html(const std::string &html) {
    Model::LiveResourceDetail detail;
    detail.source = "livingroom";

    const std::regex url_pattern(R"(https?://[^\s"'<>()[\]{}|\\^`]+)");
    for (auto it = std::sregex_iterator(html.begin(), html.end(), url_pattern); it != std::sregex_iterator(); ++it) {
        auto url = normalize_url_text((*it)[0].str());
        push_video_source(detail.video_sources, url.find(".m3u8") != std::string::npos ? "html.m3u8" : "html.url", url);
    }

    const std::regex tag_pattern(R"(<(?:video|source)[^>]+src=["']([^"']+)["'])", std::regex_constants::icase);
    for (auto it = std::sregex_iterator(html.begin(), html.end(), tag_pattern); it != std::sregex_iterator(); ++it) {
        push_video_source(detail.video_sources, "html.tag", (*it)[1].str());
    }

    const std::regex js_video_pattern(R"((?:video_url|videoUrl|play_url|playUrl|stream_url|streamUrl|src|source|url)\s*[:=]\s*["']([^"']*(?:m3u8|mp4)[^"']*)["'])",
                                      std::regex_constants::icase);
    for (auto it = std::sregex_iterator(html.begin(), html.end(), js_video_pattern); it != std::sregex_iterator(); ++it) {
        push_video_source(detail.video_sources, "html.js", (*it)[1].str());
    }

    const std::regex guid_pattern(R"([^a-fA-F0-9]([a-fA-F0-9]{32})[^a-fA-F0-9])");
    for (auto it = std::sregex_iterator(html.begin(), html.end(), guid_pattern); it != std::sregex_iterator(); ++it) {
        push_unique(detail.ppt_guids, (*it)[1].str());
    }
    const std::regex guid_query_pattern(R"(resource_guid=([a-fA-F0-9]{32}))");
    for (auto it = std::sregex_iterator(html.begin(), html.end(), guid_query_pattern); it != std::sregex_iterator(); ++it) {
        push_unique(detail.ppt_guids, (*it)[1].str());
    }

    if (!detail.video_sources.empty()) {
        detail.primary_video_url = detail.video_sources.front().url;
        detail.primary_video_hls = detail.video_sources.front().hls;
        detail.has_video = true;
        auto live = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [](const Model::LiveVideoSource &source) { return source.hls; });
        auto replay = std::find_if(detail.video_sources.begin(), detail.video_sources.end(), [](const Model::LiveVideoSource &source) { return !source.hls; });
        if (live != detail.video_sources.end()) detail.live_url = live->url;
        if (replay != detail.video_sources.end()) detail.playback_url = replay->url;
    }
    if (!detail.ppt_guids.empty()) detail.ppt_resource_guid = detail.ppt_guids.front();
    return detail;
}

std::vector<Model::LivePptSlide> parse_live_ppt_slides(const nlohmann::json &root) {
    auto list = list_from_resource_envelope(root);
    std::vector<Model::LivePptSlide> slides;
    if (!list.is_array()) return slides;
    int fallback_index = 0;
    for (const auto &item : list) {
        if (!item.is_object()) continue;
        auto content = parse_json_maybe(item.contains("content") ? item["content"] : nlohmann::json::object());
        auto image_url = first_of(content, {"pptimgurl", "img_url", "imageUrl", "url"});
        if (image_url.empty()) image_url = first_of(item, {"img_url", "imageUrl", "url"});
        if (image_url.empty()) continue;
        Model::LivePptSlide slide;
        slide.index = fallback_index++;
        slide.time_sec = item.contains("created_sec") ? json_int(item, "created_sec") : json_int(item, "time_sec");
        slide.image_url = normalize_url_text(std::move(image_url));
        slides.push_back(std::move(slide));
    }
    std::sort(slides.begin(), slides.end(), [](const Model::LivePptSlide &lhs, const Model::LivePptSlide &rhs) {
        if (lhs.time_sec == rhs.time_sec) return lhs.index < rhs.index;
        return lhs.time_sec < rhs.time_sec;
    });
    for (std::size_t i = 0; i < slides.size(); ++i) slides[i].index = static_cast<int>(i);
    return slides;
}

std::vector<std::uint8_t> build_live_pptx(const std::vector<Model::LiveBinaryResource> &images) {
    StoredZipWriter zip;
    std::ostringstream content_types;
    content_types << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                  << R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
                  << R"(<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>)"
                  << R"(<Default Extension="xml" ContentType="application/xml"/>)"
                  << R"(<Default Extension="jpg" ContentType="image/jpeg"/>)"
                  << R"(<Default Extension="jpeg" ContentType="image/jpeg"/>)"
                  << R"(<Default Extension="png" ContentType="image/png"/>)"
                  << R"(<Default Extension="gif" ContentType="image/gif"/>)"
                  << R"(<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>)";
    for (std::size_t i = 0; i < images.size(); ++i) {
        content_types << R"(<Override PartName="/ppt/slides/slide)" << (i + 1)
                      << R"(.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>)";
    }
    content_types << "</Types>";

    zip.add("[Content_Types].xml", bytes_from_string(content_types.str()));
    zip.add("_rels/.rels",
            bytes_from_string(R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/></Relationships>)"));

    std::ostringstream presentation;
    presentation << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
                 << R"(<p:presentation xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:sldIdLst>)";
    for (std::size_t i = 0; i < images.size(); ++i) {
        presentation << R"(<p:sldId id=")" << (256 + i) << R"(" r:id="rId)" << (i + 1) << R"("/>)";
    }
    presentation << R"(</p:sldIdLst><p:sldSz cx="12192000" cy="6858000" type="wide"/><p:notesSz cx="6858000" cy="9144000"/></p:presentation>)";
    zip.add("ppt/presentation.xml", bytes_from_string(presentation.str()));

    std::ostringstream presentation_rels;
    presentation_rels << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)";
    for (std::size_t i = 0; i < images.size(); ++i) {
        presentation_rels << R"(<Relationship Id="rId)" << (i + 1)
                          << R"(" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide)"
                          << (i + 1) << R"(.xml"/>)";
    }
    presentation_rels << "</Relationships>";
    zip.add("ppt/_rels/presentation.xml.rels", bytes_from_string(presentation_rels.str()));

    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto ext = image_extension(images[i], static_cast<int>(i));
        const auto image_name = "image" + std::to_string(i + 1) + ext;
        std::ostringstream slide;
        slide << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)"
              << R"(<p:sld xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:cSld><p:spTree>)"
              << R"(<p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>)"
              << R"(<p:pic><p:nvPicPr><p:cNvPr id="2" name=")" << xml_escape(images[i].name.empty() ? image_name : images[i].name)
              << R"("/><p:cNvPicPr/><p:nvPr/></p:nvPicPr><p:blipFill><a:blip r:embed="rId1"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>)"
              << R"(<p:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="12192000" cy="6858000"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic>)"
              << R"(</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>)";
        zip.add("ppt/slides/slide" + std::to_string(i + 1) + ".xml", bytes_from_string(slide.str()));
        zip.add("ppt/slides/_rels/slide" + std::to_string(i + 1) + ".xml.rels",
                bytes_from_string(R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/)" + image_name + R"("/></Relationships>)"));
        zip.add("ppt/media/" + image_name, images[i].bytes);
    }

    return zip.finish();
}

} // namespace Parser
} // namespace UBAANext
