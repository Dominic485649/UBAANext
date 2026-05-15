#include <UBAANext/Parser/YgdkParser.hpp>

#include <utility>

namespace UBAANext {
namespace Parser {
namespace {

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    return {};
}

} // namespace

std::vector<Model::YgdkClassify> parse_ygdk_classifies(const nlohmann::json &data) {
    auto list = data.contains("list") && data["list"].is_array() ? data["list"] : nlohmann::json::array();
    std::vector<Model::YgdkClassify> records;
    for (const auto &item : list) {
        Model::YgdkClassify record;
        record.id = json_string(item, "classify_id");
        record.name = json_string(item, "name");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

Model::YgdkClassify select_ygdk_sports_classify(const std::vector<Model::YgdkClassify> &classifies) {
    if (classifies.empty()) return {};
    for (const auto &classify : classifies) {
        if (classify.name.find("体育") != std::string::npos) return classify;
    }
    return classifies.front();
}

std::vector<Model::YgdkItem> parse_ygdk_items(const nlohmann::json &data, const std::string &classify_id) {
    auto list = data.contains("list") && data["list"].is_array() ? data["list"] : nlohmann::json::array();
    std::vector<Model::YgdkItem> records;
    for (const auto &item : list) {
        Model::YgdkItem record;
        record.id = json_string(item, "item_id");
        record.name = json_string(item, "name");
        record.classify_id = classify_id;
        record.sort = json_string(item, "sort");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

Model::YgdkOverview parse_ygdk_overview(const Model::YgdkClassify &classify,
                                        const nlohmann::json *term,
                                        const nlohmann::json *count) {
    Model::YgdkOverview overview;
    overview.classify = classify;
    if (term != nullptr) overview.term_name = json_string(*term, "name");
    if (count != nullptr) {
        overview.term_count = json_string(*count, "term_count");
        overview.term_good_count = json_string(*count, "term_good_count");
        overview.week_count = json_string(*count, "week_count");
        overview.month_count = json_string(*count, "month_count");
        overview.day_count = json_string(*count, "day_count");
    }
    return overview;
}

std::vector<Model::YgdkRecord> parse_ygdk_records(const nlohmann::json &data) {
    auto list = data.contains("list") && data["list"].is_array() ? data["list"] : nlohmann::json::array();
    std::vector<Model::YgdkRecord> records;
    for (const auto &item : list) {
        Model::YgdkRecord record;
        record.id = json_string(item, "record_id");
        record.item_name = json_string(item, "item_name");
        record.state = json_string(item, "state");
        record.place = json_string(item, "place");
        record.start_time = json_string(item, "start_time");
        record.end_time = json_string(item, "end_time");
        record.created_at = json_string(item, "create_time_fmt");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
