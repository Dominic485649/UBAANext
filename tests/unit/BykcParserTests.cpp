#include <UBAANext/Parser/BykcParser.hpp>

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

TEST_CASE("parse_bykc_profile 解析用户资料", "[BykcParser]") {
    auto profile = um::Parser::parse_bykc_profile(load_json_fixture("bykc/profile.json"));

    CHECK(profile.id == "user-1");
    CHECK(profile.real_name == "张三");
    CHECK(profile.employee_id == "20260001");
    CHECK(profile.student_no == "20260001");
    CHECK(profile.student_type == "本科生");
    CHECK(profile.class_code == "A101");
    CHECK(profile.college_name == "智能制造学院");
    CHECK(profile.term_name == "2025-2026学年第二学期");
}

TEST_CASE("parse_bykc_courses 解析课程状态和分类", "[BykcParser]") {
    auto courses = um::Parser::parse_bykc_courses(load_json_fixture("bykc/courses.json"));

    REQUIRE(courses.size() == 3);
    CHECK(courses[0].id == "course-1");
    CHECK(courses[0].name == "创新创业实践");
    CHECK(courses[0].status == "available");
    CHECK(courses[0].teacher == "李老师");
    CHECK(courses[0].position == "学院路校区");
    CHECK(courses[0].max_count == "30");
    CHECK(courses[0].current_count == "12");
    CHECK(courses[0].category == "通识");
    CHECK(courses[0].sub_category == "创新创业");
    CHECK(courses[0].selected == "false");

    CHECK(courses[1].status == "full");
    CHECK(courses[2].status == "selected");
}

TEST_CASE("parse_bykc_courses 忽略异常容量数字", "[BykcParser]") {
    auto courses = um::Parser::parse_bykc_courses(nlohmann::json::array({{{"id", "course-x"}, {"courseName", "异常容量"}, {"courseCurrentCount", "many"}, {"courseMaxCount", "30"}}}));

    REQUIRE(courses.size() == 1);
    CHECK(courses[0].status == "available");
}

TEST_CASE("parse_bykc_course_detail 解析课程详情", "[BykcParser]") {
    auto detail = um::Parser::parse_bykc_course_detail(load_json_fixture("bykc/course_detail.json"), "course-1");

    CHECK(detail.id == "course-1");
    CHECK(detail.name == "创新创业实践");
    CHECK(detail.status == "selected");
    CHECK(detail.teacher == "李老师");
    CHECK(detail.contact == "北航楼");
    CHECK(detail.mobile == "010-00000000");
    CHECK(detail.description == "课程说明");
    CHECK(detail.sign_config == "gps");
}

TEST_CASE("parse_bykc_course_detail 保留对象形态签到配置", "[BykcParser]") {
    nlohmann::json course = {
        {"id", "course-1"},
        {"courseName", "创新创业实践"},
        {"selected", true},
        {"courseSignConfig", nlohmann::json{{"signPointList", nlohmann::json::array({nlohmann::json{{"lat", 40.1001}, {"lng", 116.3001}, {"radius", 8.0}}})}}},
    };

    auto detail = um::Parser::parse_bykc_course_detail(course, "course-1");

    auto sign_config = nlohmann::json::parse(detail.sign_config, nullptr, false);
    REQUIRE_FALSE(sign_config.is_discarded());
    REQUIRE(sign_config["signPointList"].is_array());
    CHECK(sign_config["signPointList"].size() == 1);
}

TEST_CASE("parse_bykc_chosen_courses 解析已选课程", "[BykcParser]") {
    auto courses = um::Parser::parse_bykc_chosen_courses(load_json_fixture("bykc/chosen.json"));

    REQUIRE(courses.size() == 1);
    CHECK(courses[0].id == "chosen-1");
    CHECK(courses[0].course_id == "course-1");
    CHECK(courses[0].name == "创新创业实践");
    CHECK(courses[0].checkin == "true");
    CHECK(courses[0].pass == "false");
    CHECK(courses[0].score == "90");
}

TEST_CASE("parse_bykc_stats 解析统计数据", "[BykcParser]") {
    auto stats = um::Parser::parse_bykc_stats(load_json_fixture("bykc/stats.json"));

    REQUIRE(stats.size() == 3);
    CHECK(stats[0].id == "total");
    CHECK(stats[0].valid_count == "3");
    CHECK(stats[1].id == "通识:创新创业");
    CHECK(stats[1].required_count == "2");
    CHECK(stats[1].passed_count == "1");
}
