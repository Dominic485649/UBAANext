#pragma once

#include <UBAANext/Model/Cloud.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** ReadOnlyCandidate parser: maps Anyshare root directory JSON into stable Core cloud items. */
std::vector<Model::CloudItem> parse_cloud_roots(const nlohmann::json &root);

/** ReadOnlyCandidate parser: maps Anyshare directory listing JSON into stable Core cloud dir model. */
Model::CloudDir parse_cloud_dir(const nlohmann::json &root);

/** ReadOnlyCandidate parser: maps Anyshare directory size JSON into stable Core cloud size model. */
Model::CloudSize parse_cloud_size(const nlohmann::json &root);

/** ReadOnlyCandidate parser: maps Anyshare share history JSON into stable Core share records. */
std::vector<Model::CloudShare> parse_cloud_shares(const nlohmann::json &root);

} // namespace Parser
} // namespace UBAANext
