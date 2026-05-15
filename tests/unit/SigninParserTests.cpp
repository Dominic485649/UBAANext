#include <UBAANext/Parser/SigninParser.hpp>

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

TEST_CASE("parse_signin_today_courses 解析今日签到课程", "[SigninParser]") {
    auto records = um::Parser::parse_signin_today_courses(load_json_fixture("signin/today.json"));

    REQUIRE(records.size() == 3);
    CHECK(records[0].id == "sched-1");
    CHECK(records[0].name == "智能制造导论");
    CHECK(records[0].status == "available");
    CHECK(records[0].sign_status == "0");
    CHECK(records[0].class_begin_time == "08:00");
    CHECK(records[0].class_end_time == "09:40");

    CHECK(records[1].id == "sched-2");
    CHECK(records[1].status == "signed");
    CHECK(records[1].sign_status == "1");

    CHECK(records[2].id == "无 ID 课程");
    CHECK(records[2].name == "无 ID 课程");
    CHECK(records[2].status == "available");
}

TEST_CASE("parse_signin_today_courses 缺少 result 返回空列表", "[SigninParser]") {
    auto records = um::Parser::parse_signin_today_courses(nlohmann::json::object());

    CHECK(records.empty());
}
