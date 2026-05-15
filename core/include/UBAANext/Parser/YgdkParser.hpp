#pragma once

#include <UBAANext/Model/Ygdk.hpp>

#include <nlohmann/json.hpp>

#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::YgdkClassify> parse_ygdk_classifies(const nlohmann::json &data);
Model::YgdkClassify select_ygdk_sports_classify(const std::vector<Model::YgdkClassify> &classifies);
std::vector<Model::YgdkItem> parse_ygdk_items(const nlohmann::json &data, const std::string &classify_id);
Model::YgdkOverview parse_ygdk_overview(const Model::YgdkClassify &classify,
                                        const nlohmann::json *term,
                                        const nlohmann::json *count);
std::vector<Model::YgdkRecord> parse_ygdk_records(const nlohmann::json &data);

} // namespace Parser
} // namespace UBAANext
