#include <UBAANext/Parser/LibrarySeatParser.hpp>

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

std::string title_or(const std::string &value, const std::string &fallback) {
    return value.empty() ? fallback : value;
}

} // namespace

std::vector<Model::LibraryInfo> parse_library_infos(const nlohmann::json &list) {
    std::vector<Model::LibraryInfo> records;
    if (!list.is_array()) return records;
    for (const auto &raw : list) {
        if (!raw.is_object()) continue;
        Model::LibraryInfo record;
        record.id = json_string(raw, "id");
        record.name = title_or(json_string(raw, "name"), "图书馆");
        record.free_num = json_string(raw, "free_num");
        record.total_num = json_string(raw, "total_num");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::LibraryArea> parse_library_areas(const nlohmann::json &list) {
    std::vector<Model::LibraryArea> records;
    if (!list.is_array()) return records;
    for (const auto &raw : list) {
        if (!raw.is_object()) continue;
        Model::LibraryArea record;
        record.id = json_string(raw, "id");
        record.name = title_or(json_string(raw, "name"), "图书馆区域");
        record.area = json_string(raw, "area");
        record.premises_id = json_string(raw, "premises_id");
        record.storey_id = json_string(raw, "storey_id");
        record.free_num = json_string(raw, "free_num");
        record.total_num = json_string(raw, "total_num");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

Model::LibraryArea parse_library_area_detail(const nlohmann::json &data, const std::string &area_id) {
    auto area = data.contains("area") && data["area"].is_object() ? data["area"] : nlohmann::json::object();
    Model::LibraryArea record;
    record.id = json_string(area, "id").empty() ? area_id : json_string(area, "id");
    record.name = json_string(area, "name");
    if (data.contains("date") && data["date"].is_object() && data["date"].contains("list") && data["date"]["list"].is_array()) {
        record.available_dates = std::to_string(data["date"]["list"].size());
    } else {
        record.available_dates = "0";
    }
    return record;
}

std::vector<Model::LibrarySeat> parse_library_seats(const nlohmann::json &list) {
    std::vector<Model::LibrarySeat> records;
    if (!list.is_array()) return records;
    for (const auto &raw : list) {
        if (!raw.is_object()) continue;
        Model::LibrarySeat record;
        record.id = json_string(raw, "id");
        auto no = json_string(raw, "no");
        record.title = no.empty() ? json_string(raw, "name") : no;
        record.name = json_string(raw, "name");
        record.raw_status = json_string(raw, "status");
        record.status = record.raw_status == "1" ? "available" : "unavailable";
        record.status_name = json_string(raw, "status_name");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::LibraryReservation> parse_library_reservations(const nlohmann::json &list) {
    std::vector<Model::LibraryReservation> records;
    if (!list.is_array()) return records;
    for (const auto &raw : list) {
        if (!raw.is_object()) continue;
        Model::LibraryReservation record;
        record.id = json_string(raw, "id");
        auto seat_no = json_string(raw, "no");
        record.title = seat_no.empty() ? "图书馆预约" : seat_no;
        record.status = json_string(raw, "status");
        record.area_name = json_string(raw, "name");
        record.day = json_string(raw, "day");
        record.begin_time = json_string(raw, "beginTime");
        record.end_time = json_string(raw, "endTime");
        record.status_name = json_string(raw, "status_name");
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
