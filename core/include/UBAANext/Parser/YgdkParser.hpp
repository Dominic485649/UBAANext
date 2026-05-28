#pragma once

#include <UBAANext/Model/Ygdk.hpp>

#include <nlohmann/json.hpp>

#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser entry: maps YGDK classify JSON; sports-domain enum drift remains possible. */
std::vector<Model::YgdkClassify> parse_ygdk_classifies(const nlohmann::json &data);
/** PartiallyMigrated helper: chooses the sports classify from backend data without proving all enum variants. */
Model::YgdkClassify select_ygdk_sports_classify(const std::vector<Model::YgdkClassify> &classifies);
/** Sensitive output: parses YGDK item metadata for one classify. */
std::vector<Model::YgdkItem> parse_ygdk_items(const nlohmann::json &data, const std::string &classify_id);
/** Sensitive output: parses sports/health overview data and must not be logged verbatim. */
Model::YgdkOverview parse_ygdk_overview(const Model::YgdkClassify &classify,
                                        const nlohmann::json *term,
                                        const nlohmann::json *count);
/** Sensitive output: parses clock-in records including time/location-like fields. */
std::vector<Model::YgdkRecord> parse_ygdk_records(const nlohmann::json &data);

} // namespace Parser
} // namespace UBAANext
