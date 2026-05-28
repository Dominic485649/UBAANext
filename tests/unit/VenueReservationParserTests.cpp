#include <UBAANext/Parser/VenueReservationParser.hpp>

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

TEST_CASE("parse_venue_sites 解析场馆站点", "[VenueReservationParser]") {
    auto sites = um::Parser::parse_venue_sites(load_json_fixture("venue/sites.json"));

    REQUIRE(sites.size() == 2);
    CHECK(sites[0].id == "site-1");
    CHECK(sites[0].name == "第一会议室");
    CHECK(sites[0].venue_id == "venue-1");
    CHECK(sites[0].venue_name == "学院路研讨室");
    CHECK(sites[0].campus_name == "学院路校区");
}

TEST_CASE("parse_venue_purpose_types 解析用途类型", "[VenueReservationParser]") {
    auto purposes = um::Parser::parse_venue_purpose_types(load_json_fixture("venue/codes.json"));

    REQUIRE(purposes.size() == 2);
    CHECK(purposes[0].id == "1");
    CHECK(purposes[0].name == "导学活动类");
    CHECK(purposes[1].id == "2");
    CHECK(purposes[1].name == "学术研讨类");
}

TEST_CASE("parse_venue_day_info 解析可预约空间", "[VenueReservationParser]") {
    auto spaces = um::Parser::parse_venue_day_info(load_json_fixture("venue/day_info.json"), "site-1");

    REQUIRE(spaces.size() == 3);
    CHECK(spaces[0].id == "space-1:1001");
    CHECK(spaces[0].name == "A101");
    CHECK(spaces[0].date == "2026-05-15");
    CHECK(spaces[0].site_id == "site-1");
    CHECK(spaces[0].token == "token-1");
    CHECK(spaces[0].time_id == "1001");
    CHECK(spaces[0].time_label == "08:00-10:00");
    CHECK(spaces[0].reservable == "true");
    CHECK(spaces[1].reservable == "false");
    CHECK(spaces[2].id == "space-2");
}

TEST_CASE("parse_venue_orders 解析订单列表", "[VenueReservationParser]") {
    auto orders = um::Parser::parse_venue_orders(load_json_fixture("venue/orders.json"));

    REQUIRE(orders.size() == 1);
    CHECK(orders[0].id == "order-1");
    CHECK(orders[0].title == "组会");
    CHECK(orders[0].status == "approved");
    CHECK(orders[0].reservation_date == "2026-05-15");
    CHECK(orders[0].space == "A101");
    CHECK(orders[0].site == "第一会议室");
}

TEST_CASE("parse_venue_order_detail 解析订单详情", "[VenueReservationParser]") {
    auto order = um::Parser::parse_venue_order_detail(load_json_fixture("venue/order_detail.json"), "order-override");

    CHECK(order.id == "order-override");
    CHECK(order.title == "组会");
    CHECK(order.status == "approved");
    CHECK(order.phone == "13800000000");
    CHECK(order.joiners == "张三,李四");
}

TEST_CASE("parse_venue_sites 对空数据和字段漂移稳定降级", "[VenueReservationParser][contract]") {
    CHECK(um::Parser::parse_venue_sites(nlohmann::json::object({{"content", "not-array"}})).empty());

    auto sites = um::Parser::parse_venue_sites(nlohmann::json{{"content", nlohmann::json::array({
        {{"id", "venue-1"}, {"venueName", 8}, {"campusName", nlohmann::json::array({"bad"})}, {"siteList", nlohmann::json::array({
            {{"id", nullptr}, {"siteName", "缺少 id"}},
            {{"id", nlohmann::json::array({"bad"})}, {"siteName", "数组 id"}},
            {{"id", 42}, {"siteName", nlohmann::json::object({{"bad", true}})}},
        })}},
    })}});

    REQUIRE(sites.size() == 1);
    CHECK(sites[0].id == "42");
    CHECK(sites[0].name.empty());
    CHECK(sites[0].venue_name == "8");
    CHECK(sites[0].campus_name.empty());
}

TEST_CASE("parse_venue_day_info 对空数据和时间段漂移稳定降级", "[VenueReservationParser][contract]") {
    CHECK(um::Parser::parse_venue_day_info(nlohmann::json::object({{"reservationDateSpaceInfo", "not-object"}}), "site-1").empty());

    auto spaces = um::Parser::parse_venue_day_info(nlohmann::json{
        {"spaceTimeInfo", nlohmann::json::array({{{"id", 1001}, {"beginTime", "08:00"}, {"endTime", "10:00"}}})},
        {"reservationDateSpaceInfo", nlohmann::json{{"20260515", nlohmann::json::array({
            {{"id", nullptr}, {"spaceName", "缺少 id"}},
            {{"id", nlohmann::json::array({"bad"})}, {"spaceName", "数组 id"}},
            {{"id", 9}, {"spaceName", nlohmann::json::object({{"bad", true}})}, {"1001", nlohmann::json{{"reservationStatus", "1"}}}},
        })}}},
    }, "site-1");

    REQUIRE(spaces.size() == 1);
    CHECK(spaces[0].id == "9:1001");
    CHECK(spaces[0].name.empty());
    CHECK(spaces[0].date == "20260515");
    CHECK(spaces[0].site_id == "site-1");
    CHECK(spaces[0].reservable == "true");
}

TEST_CASE("parse_venue_orders 对空数据和无 id 记录稳定处理", "[VenueReservationParser][contract]") {
    CHECK(um::Parser::parse_venue_orders(nlohmann::json::object({{"content", "not-array"}})).empty());

    auto orders = um::Parser::parse_venue_orders(nlohmann::json{{"content", nlohmann::json::array({
        {{"id", nullptr}, {"topic", "缺少 id"}},
        {{"id", nlohmann::json::object({{"bad", true}})}, {"topic", "对象 id"}},
        {{"id", 6}, {"theme", nlohmann::json::array({"bad"})}, {"orderStatus", 3}, {"reservationDate", 20260515}, {"venueSpaceName", nlohmann::json::object({{"bad", true}})}},
    })}});

    REQUIRE(orders.size() == 1);
    CHECK(orders[0].id == "6");
    CHECK(orders[0].title.empty());
    CHECK(orders[0].status == "3");
    CHECK(orders[0].reservation_date == "20260515");
    CHECK(orders[0].space.empty());
}
