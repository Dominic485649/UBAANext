#include <UBAANext/Parser/LibrarySeatParser.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace um = UBAANext;

namespace {

std::string load_fixture(const std::string &relative_path) {
    std::ifstream input(std::filesystem::path(UBAA_TEST_FIXTURES_DIR) / relative_path, std::ios::binary);
    REQUIRE(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

nlohmann::json load_json_fixture(const std::string &relative_path) {
    auto json = nlohmann::json::parse(load_fixture(relative_path), nullptr, false);
    REQUIRE_FALSE(json.is_discarded());
    return json;
}

} // namespace

TEST_CASE("parse_library_infos 解析图书馆列表", "[LibrarySeatParser]") {
    auto libraries = um::Parser::parse_library_infos(load_json_fixture("library/libraries.json"));

    REQUIRE(libraries.size() == 2);
    CHECK(libraries[0].id == "lib-1");
    CHECK(libraries[0].name == "北区图书馆");
    CHECK(libraries[0].free_num == "12");
    CHECK(libraries[0].total_num == "100");
    CHECK(libraries[1].free_num == "8");
}

TEST_CASE("parse_library_areas 解析区域列表", "[LibrarySeatParser]") {
    auto areas = um::Parser::parse_library_areas(load_json_fixture("library/areas.json"));

    REQUIRE(areas.size() == 1);
    CHECK(areas[0].id == "area-1");
    CHECK(areas[0].name == "三层阅览区");
    CHECK(areas[0].area == "A");
    CHECK(areas[0].premises_id == "lib-1");
    CHECK(areas[0].storey_id == "3");
    CHECK(areas[0].free_num == "6");
    CHECK(areas[0].total_num == "20");
}

TEST_CASE("parse_library_areas 接受 camelCase 字段", "[LibrarySeatParser]") {
    auto areas = um::Parser::parse_library_areas(nlohmann::json::array({{{"id", "area-2"}, {"name", "四层阅览区"}, {"areaName", "B"}, {"premisesId", "lib-2"}, {"storeyId", "4"}, {"freeNum", 3}, {"totalNum", 18}}}));

    REQUIRE(areas.size() == 1);
    CHECK(areas[0].area == "B");
    CHECK(areas[0].premises_id == "lib-2");
    CHECK(areas[0].storey_id == "4");
    CHECK(areas[0].free_num == "3");
    CHECK(areas[0].total_num == "18");
}

TEST_CASE("parse_library_area_detail 解析区域详情", "[LibrarySeatParser]") {
    auto area = um::Parser::parse_library_area_detail(load_json_fixture("library/area_detail.json"), "fallback-area");

    CHECK(area.id == "area-1");
    CHECK(area.name == "三层阅览区");
    CHECK(area.available_dates == "2");
}

TEST_CASE("parse_library_seats 解析座位列表", "[LibrarySeatParser]") {
    auto seats = um::Parser::parse_library_seats(load_json_fixture("library/seats.json"));

    REQUIRE(seats.size() == 2);
    CHECK(seats[0].id == "seat-1");
    CHECK(seats[0].title == "A001");
    CHECK(seats[0].status == "available");
    CHECK(seats[0].raw_status == "1");
    CHECK(seats[0].status_name == "可预约");
    CHECK(seats[1].status == "unavailable");
}

TEST_CASE("parse_library_reservations 解析预约记录", "[LibrarySeatParser]") {
    auto reservations = um::Parser::parse_library_reservations(load_json_fixture("library/reservations.json"));

    REQUIRE(reservations.size() == 1);
    CHECK(reservations[0].id == "booking-1");
    CHECK(reservations[0].title == "A001");
    CHECK(reservations[0].status == "reserved");
    CHECK(reservations[0].area_name == "三层阅览区");
    CHECK(reservations[0].day == "2026-05-15");
    CHECK(reservations[0].begin_time == "08:00");
    CHECK(reservations[0].end_time == "10:00");
    CHECK(reservations[0].status_name == "预约成功");
}

TEST_CASE("parse_library_reservations 接受 snake_case 字段别名", "[LibrarySeatParser]") {
    auto reservations = um::Parser::parse_library_reservations(nlohmann::json::array({{{"id", "booking-2"}, {"seat_no", "B012"}, {"status", "6"}, {"area_name", "四层阅览区"}, {"date", "2026-05-16"}, {"begin_time", "10:00"}, {"end_time", "12:00"}, {"statusName", "已结束"}}}));

    REQUIRE(reservations.size() == 1);
    CHECK(reservations[0].title == "B012");
    CHECK(reservations[0].area_name == "四层阅览区");
    CHECK(reservations[0].day == "2026-05-16");
    CHECK(reservations[0].begin_time == "10:00");
    CHECK(reservations[0].end_time == "12:00");
    CHECK(reservations[0].status_name == "已结束");
}

TEST_CASE("parse_library_infos 对空数据和字段漂移稳定降级", "[LibrarySeatParser][contract]") {
    CHECK(um::Parser::parse_library_infos(nlohmann::json::object({{"content", "not-array"}})).empty());

    auto libraries = um::Parser::parse_library_infos(nlohmann::json::array({
        {{"id", nullptr}, {"name", "缺少 id"}},
        {{"id", nlohmann::json::object({{"bad", true}})}, {"name", "对象 id"}},
        {{"id", 3}, {"name", nlohmann::json::array({"bad"})}, {"freeNum", 8}, {"totalNum", nlohmann::json::object({{"bad", true}})}},
    }));

    REQUIRE(libraries.size() == 1);
    CHECK(libraries[0].id == "3");
    CHECK(libraries[0].name == "图书馆");
    CHECK(libraries[0].free_num == "8");
    CHECK(libraries[0].total_num.empty());
}

TEST_CASE("parse_library_areas 对空数据和无 id 记录稳定处理", "[LibrarySeatParser][contract]") {
    CHECK(um::Parser::parse_library_areas(nlohmann::json::object({{"content", "not-array"}})).empty());

    auto areas = um::Parser::parse_library_areas(nlohmann::json::array({
        {{"id", nullptr}, {"name", "缺少 id"}},
        {{"id", nlohmann::json::array({"bad"})}, {"name", "数组 id"}},
        {{"id", 4}, {"name", nlohmann::json::object({{"bad", true}})}, {"premises_id", 3}, {"storey_id", nlohmann::json::array({"bad"})}, {"free_num", 6}},
    }));

    REQUIRE(areas.size() == 1);
    CHECK(areas[0].id == "4");
    CHECK(areas[0].name == "图书馆区域");
    CHECK(areas[0].premises_id == "3");
    CHECK(areas[0].storey_id.empty());
    CHECK(areas[0].free_num == "6");
}

TEST_CASE("parse_library_area_detail 对字段漂移保留 fallback", "[LibrarySeatParser][contract]") {
    auto area = um::Parser::parse_library_area_detail(nlohmann::json{{"id", nlohmann::json::array({"bad"})}, {"name", nlohmann::json::object({{"bad", true}})}, {"openDates", "not-array"}}, "fallback-area");

    CHECK(area.id == "fallback-area");
    CHECK(area.name == "图书馆区域");
    CHECK(area.available_dates == "0");
}

TEST_CASE("parse_library_seats 和 reservations 跳过无 id 敏感记录", "[LibrarySeatParser][contract]") {
    auto seats = um::Parser::parse_library_seats(nlohmann::json::array({
        {{"id", nullptr}, {"seatNo", "A001"}},
        {{"id", nlohmann::json::object({{"bad", true}})}, {"seatNo", "A002"}},
        {{"id", 5}, {"seatNo", nlohmann::json::array({"bad"})}, {"status", 1}, {"statusName", nlohmann::json::object({{"bad", true}})}},
    }));

    REQUIRE(seats.size() == 1);
    CHECK(seats[0].id == "5");
    CHECK(seats[0].title == "座位");
    CHECK(seats[0].raw_status == "1");
    CHECK(seats[0].status_name.empty());

    auto reservations = um::Parser::parse_library_reservations(nlohmann::json::array({
        {{"id", nullptr}, {"seatNo", "A001"}},
        {{"id", nlohmann::json::array({"bad"})}, {"seatNo", "A002"}},
        {{"id", 6}, {"seatNo", nlohmann::json::object({{"bad", true}})}, {"status", 2}, {"date", 20260515}, {"areaName", nlohmann::json::array({"bad"})}},
    }));

    REQUIRE(reservations.size() == 1);
    CHECK(reservations[0].id == "6");
    CHECK(reservations[0].title == "图书馆预约");
    CHECK(reservations[0].status == "2");
    CHECK(reservations[0].day == "20260515");
    CHECK(reservations[0].area_name.empty());
}
