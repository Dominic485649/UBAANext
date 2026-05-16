#include <UBAANext/Parser/LibrarySeatParser.hpp>

#include <initializer_list>
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

std::string json_string_any(const nlohmann::json &json, std::initializer_list<const char *> keys) {
    for (auto *key : keys) {
        auto value = json_string(json, key);
        if (!value.empty()) return value;
    }
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
        record.free_num = json_string_any(raw, {"free_num", "freeNum"});
        record.total_num = json_string_any(raw, {"total_num", "totalNum"});
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
        record.area = json_string_any(raw, {"area", "areaName"});
        record.premises_id = json_string_any(raw, {"premises_id", "premisesId"});
        record.storey_id = json_string_any(raw, {"storey_id", "storeyId"});
        record.free_num = json_string_any(raw, {"free_num", "freeNum"});
        record.total_num = json_string_any(raw, {"total_num", "totalNum"});
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
        auto no = json_string_any(raw, {"no", "seat_no", "seatNo"});
        record.title = no.empty() ? json_string(raw, "name") : no;
        record.name = json_string(raw, "name");
        record.raw_status = json_string(raw, "status");
        record.status = record.raw_status == "1" ? "available" : "unavailable";
        record.status_name = json_string_any(raw, {"status_name", "statusName"});
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
        auto seat_no = json_string_any(raw, {"no", "seat_no", "seatNo"});
        record.title = seat_no.empty() ? "图书馆预约" : seat_no;
        record.status = json_string(raw, "status");
        record.area_name = json_string_any(raw, {"name", "area_name", "areaName"});
        record.day = json_string_any(raw, {"day", "date"});
        record.begin_time = json_string_any(raw, {"beginTime", "begin_time"});
        record.end_time = json_string_any(raw, {"endTime", "end_time"});
        record.status_name = json_string_any(raw, {"status_name", "statusName"});
        if (!record.id.empty()) records.push_back(std::move(record));
    }
    return records;
}

} // namespace Parser
} // namespace UBAANext
