#include <UBAANext/Parser/YgdkParser.hpp>

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

TEST_CASE("parse_ygdk_classifies 解析并选择体育分类", "[YgdkParser]") {
    auto classifies = um::Parser::parse_ygdk_classifies(load_json_fixture("ygdk/classifies.json"));

    REQUIRE(classifies.size() == 2);
    CHECK(classifies[0].id == "c1");
    CHECK(classifies[0].name == "劳动教育");

    auto selected = um::Parser::select_ygdk_sports_classify(classifies);
    CHECK(selected.id == "c2");
    CHECK(selected.name == "体育锻炼");
}

TEST_CASE("parse_ygdk_items 解析打卡项目", "[YgdkParser]") {
    auto items = um::Parser::parse_ygdk_items(load_json_fixture("ygdk/items.json"), "c2");

    REQUIRE(items.size() == 2);
    CHECK(items[0].id == "item-1");
    CHECK(items[0].name == "跑步");
    CHECK(items[0].classify_id == "c2");
    CHECK(items[0].sort == "1");
    CHECK(items[1].sort == "2");
}

TEST_CASE("parse_ygdk_overview 解析统计概览", "[YgdkParser]") {
    um::Model::YgdkClassify classify{"c2", "体育锻炼"};
    auto term = load_json_fixture("ygdk/term.json");
    auto count = load_json_fixture("ygdk/count.json");

    auto overview = um::Parser::parse_ygdk_overview(classify, &term, &count);

    CHECK(overview.classify.id == "c2");
    CHECK(overview.classify.name == "体育锻炼");
    CHECK(overview.term_name == "2025-2026学年第二学期");
    CHECK(overview.term_count == "20");
    CHECK(overview.term_good_count == "8");
    CHECK(overview.week_count == "3");
    CHECK(overview.month_count == "10");
    CHECK(overview.day_count == "1");
}

TEST_CASE("parse_ygdk_records 解析打卡记录", "[YgdkParser]") {
    auto records = um::Parser::parse_ygdk_records(load_json_fixture("ygdk/records.json"));

    REQUIRE(records.size() == 1);
    CHECK(records[0].id == "record-1");
    CHECK(records[0].item_name == "跑步");
    CHECK(records[0].state == "approved");
    CHECK(records[0].place == "操场");
    CHECK(records[0].start_time == "2026-03-01 08:00");
    CHECK(records[0].end_time == "2026-03-01 09:00");
    CHECK(records[0].created_at == "2026-03-01 09:05");
}
