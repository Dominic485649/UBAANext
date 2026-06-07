#pragma once

#include <UBAANext/Model/Live.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser: maps classroom live weekly schedule JSON; backend envelope drift must fail closed in service. */
std::vector<std::vector<Model::LiveSchedule>> parse_live_week_schedule_days(const nlohmann::json &list);

/** ReadOnlyCandidate parser: flattens classroom resource search responses from live/yjapi/searchlist style envelopes. */
std::vector<Model::LiveResource> parse_live_resources(const nlohmann::json &root, const std::string &source = "live");

/** ReadOnlyCandidate parser: extracts video URLs, PPT GUIDs, and status fields from one classroom resource item. */
Model::LiveResourceDetail parse_live_resource_detail(const nlohmann::json &item);

/** ReadOnlyCandidate parser: extracts video URLs and PPT GUIDs from livingroom HTML/embedded JS. */
Model::LiveResourceDetail parse_live_livingroom_html(const std::string &html);

/** ReadOnlyCandidate parser: extracts sorted PPT slide timeline from pptnote response envelopes. */
std::vector<Model::LivePptSlide> parse_live_ppt_slides(const nlohmann::json &root);

/** Utility: build a minimal 16:9 PPTX package from already downloaded slide images. */
std::vector<std::uint8_t> build_live_pptx(const std::vector<Model::LiveBinaryResource> &images);

} // namespace Parser
} // namespace UBAANext
