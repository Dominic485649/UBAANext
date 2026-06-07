#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace UBAANext {
namespace Model {

struct LiveSchedule {
    std::string course_id;
    std::string live_id;
    std::string name;
    std::string teacher;
    std::string raw_status;
};

struct LiveWeekSchedule {
    std::string start_date;
    std::string end_date;
    std::vector<std::vector<LiveSchedule>> days;
};

struct LiveResourceQuery {
    std::string date;
    std::string status = "all";
    bool from_course = false;
    int page = 1;
    int size = 100;
};

struct LiveVideoSource {
    std::string kind;
    std::string url;
    bool hls = false;
};

struct LiveResource {
    std::string course_id;
    std::string sub_id;
    std::string title;
    std::string course_code;
    std::string teacher;
    std::string room;
    std::string sub_title;
    std::string status_label;
    std::string raw_status;
    std::string term_name;
    std::string course_time;
    std::string time_slot;
    std::string time_range;
    std::string thumb_url;
    std::string source;
};

struct LiveResourceDetail : LiveResource {
    bool has_video = false;
    std::string primary_video_url;
    bool primary_video_hls = false;
    std::string live_url;
    std::string playback_url;
    bool playback_hls = false;
    std::string ppt_video_url;
    std::string sub_resource_guid;
    std::string ppt_resource_guid;
    std::vector<std::string> ppt_guids;
    std::vector<LiveVideoSource> video_sources;
};

struct LivePptSlide {
    int index = 0;
    int time_sec = 0;
    std::string image_url;
};

struct LiveBinaryResource {
    std::string name;
    std::string content_type;
    std::vector<std::uint8_t> bytes;
};

struct LiveDownloadResult {
    std::string course_id;
    std::string sub_id;
    std::string title;
    std::string status = "empty";
    std::string used_guid;
    std::string message;
    std::vector<LivePptSlide> slides;
    std::vector<LiveBinaryResource> images;
    std::vector<std::string> failed_images;
    std::vector<std::uint8_t> pptx_bytes;
    std::string video_url;
    bool video_hls = false;
    std::vector<std::uint8_t> video_bytes;
};

} // namespace Model
} // namespace UBAANext
