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
