#pragma once

#include <UBAANext/Model/Live.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser: maps classroom live weekly schedule JSON; backend envelope drift must fail closed in service. */
std::vector<std::vector<Model::LiveSchedule>> parse_live_week_schedule_days(const nlohmann::json &list);

} // namespace Parser
} // namespace UBAANext
