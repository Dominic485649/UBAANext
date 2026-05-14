#pragma once

#include <map>
#include <string>
#include <vector>

namespace UBAANext::Model {

struct FeatureRecord {
    std::string id;
    std::string title;
    std::string status;
    std::map<std::string, std::string> fields;
};

struct MutationResult {
    bool accepted = false;
    std::string message;
    FeatureRecord summary;
};

} // namespace UBAANext::Model
