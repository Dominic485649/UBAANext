/**
 * @file JsonParserTests.cpp
 * @brief JsonParser 解析函数的单元测试
 */

#include <UBAANext/Parser/JsonParser.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

// ── parse_courses 测试 ──────────────────────────────────────────────

TEST_CASE("parse_courses 正常 JSON 解析 3 门课程", "[JsonParser]") {
    const char *json = R"([
      {"id":"C001","name":"高等数学","teacher":"张教授","classroom":"J3-101","weekStart":1,"weekEnd":16,"dayOfWeek":1,"sectionStart":1,"sectionEnd":2,"courseCode":"MATH101","credit":"3.0","beginTime":"08:00","endTime":"09:40"},
      {"id":"C002","name":"程序设计","teacher":"李教授","classroom":"J3-202","weekStart":1,"weekEnd":16,"dayOfWeek":2,"sectionStart":3,"sectionEnd":4,"courseCode":"CS101","credit":"2.0","beginTime":"10:00","endTime":"11:40"},
      {"id":"C003","name":"大学物理","teacher":"王教授","classroom":"J3-303","weekStart":1,"weekEnd":16,"dayOfWeek":3,"sectionStart":5,"sectionEnd":6,"courseCode":"PHYS101","credit":"3.0","beginTime":"14:00","endTime":"15:40"}
    ])";

    auto result = um::Parser::parse_courses(json);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);

    CHECK((*result)[0].id == "C001");
    CHECK((*result)[0].name == "高等数学");
    CHECK((*result)[0].teacher == "张教授");
    CHECK((*result)[0].classroom == "J3-101");
    CHECK((*result)[0].week_start == 1);
    CHECK((*result)[0].week_end == 16);
    CHECK((*result)[0].day_of_week == 1);
    CHECK((*result)[0].section_start == 1);
    CHECK((*result)[0].section_end == 2);
    CHECK((*result)[0].course_code == "MATH101");
    CHECK((*result)[0].credit == "3.0");
    CHECK((*result)[0].begin_time == "08:00");
    CHECK((*result)[0].end_time == "09:40");
}

TEST_CASE("parse_courses 空数组返回空列表", "[JsonParser]") {
    auto result = um::Parser::parse_courses("[]");
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("parse_courses 非法 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_courses("{invalid json");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}

TEST_CASE("parse_courses 非数组 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_courses(R"({"key":"value"})");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}

TEST_CASE("parse_courses 缺失字段使用默认值", "[JsonParser]") {
    const char *json = R"([{"id":"C001"}])";
    auto result = um::Parser::parse_courses(json);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "C001");
    CHECK((*result)[0].name.empty());
    CHECK((*result)[0].week_start == 0);
}

// ── parse_exams 测试 ────────────────────────────────────────────────

TEST_CASE("parse_exams 正常 JSON 解析 3 场考试", "[JsonParser]") {
    const char *json = R"([
      {"id":"E001","courseName":"高等数学","location":"J3-101","timeText":"2026-06-20 09:00-11:00","courseNo":"MATH101","examDate":"2026-06-20","startTime":"09:00","endTime":"11:00","seatNo":"15","examType":"期末考试","status":1},
      {"id":"E002","courseName":"程序设计","location":"J3-202","timeText":"2026-06-22 14:00-16:00","courseNo":"CS101","examDate":"2026-06-22","startTime":"14:00","endTime":"16:00","seatNo":"23","examType":"期末考试","status":1},
      {"id":"E003","courseName":"大学物理","location":"J3-303","timeText":"2026-06-24 09:00-11:00","courseNo":"PHYS101","examDate":"2026-06-24","startTime":"09:00","endTime":"11:00","seatNo":"8","examType":"期末考试","status":0}
    ])";

    auto result = um::Parser::parse_exams(json);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);

    CHECK((*result)[0].id == "E001");
    CHECK((*result)[0].course_name == "高等数学");
    CHECK((*result)[0].location == "J3-101");
    CHECK((*result)[0].status == um::Model::ExamStatus::Arranged);
    CHECK((*result)[2].status == um::Model::ExamStatus::Pending);
}

TEST_CASE("parse_exams 状态枚举值正确映射", "[JsonParser]") {
    const char *json = R"([
      {"id":"E1","courseName":"A","location":"L","timeText":"T","courseNo":"C","examDate":"D","startTime":"S","endTime":"E","seatNo":"1","examType":"T","status":0},
      {"id":"E2","courseName":"B","location":"L","timeText":"T","courseNo":"C","examDate":"D","startTime":"S","endTime":"E","seatNo":"2","examType":"T","status":1},
      {"id":"E3","courseName":"C","location":"L","timeText":"T","courseNo":"C","examDate":"D","startTime":"S","endTime":"E","seatNo":"3","examType":"T","status":2}
    ])";

    auto result = um::Parser::parse_exams(json);
    REQUIRE(result.has_value());
    CHECK((*result)[0].status == um::Model::ExamStatus::Pending);
    CHECK((*result)[1].status == um::Model::ExamStatus::Arranged);
    CHECK((*result)[2].status == um::Model::ExamStatus::Finished);
}

TEST_CASE("parse_exams 非法 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_exams("not json");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}

// ── parse_classrooms 测试 ──────────────────────────────────────────

TEST_CASE("parse_classrooms 正常 JSON 解析教室数据", "[JsonParser]") {
    const char *json = R"({
      "buildings": {
        "J3": [
          {"id":"J3-101","name":"J3-101","floorId":"1","freeSections":[1,2,3,4]},
          {"id":"J3-202","name":"J3-202","floorId":"2","freeSections":[5,6]}
        ]
      }
    })";

    auto result = um::Parser::parse_classrooms(json);
    REQUIRE(result.has_value());
    CHECK(result->buildings.size() == 1);
    REQUIRE(result->buildings.count("J3") == 1);
    REQUIRE(result->buildings.at("J3").size() == 2);
    CHECK(result->buildings.at("J3")[0].id == "J3-101");
    CHECK(result->buildings.at("J3")[0].free_sections == std::vector<int>{1, 2, 3, 4});
    CHECK(result->buildings.at("J3")[1].free_sections == std::vector<int>{5, 6});
}

TEST_CASE("parse_classrooms 空 buildings 返回空结果", "[JsonParser]") {
    auto result = um::Parser::parse_classrooms(R"({"buildings":{}})");
    REQUIRE(result.has_value());
    CHECK(result->buildings.empty());
}

TEST_CASE("parse_classrooms 非法 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_classrooms("{bad");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}

// ── parse_terms 测试 ────────────────────────────────────────────────

TEST_CASE("parse_terms 正常 JSON 解析学期列表", "[JsonParser]") {
    const char *json = R"([
      {"code":"2025-2026-1","name":"2025-2026学年第一学期","selected":false,"index":0},
      {"code":"2025-2026-2","name":"2025-2026学年第二学期","selected":true,"index":1}
    ])";

    auto result = um::Parser::parse_terms(json);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
    CHECK((*result)[0].code == "2025-2026-1");
    CHECK((*result)[0].selected == false);
    CHECK((*result)[1].selected == true);
    CHECK((*result)[1].index == 1);
}

TEST_CASE("parse_terms 非法 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_terms("???");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}

// ── parse_weeks 测试 ────────────────────────────────────────────────

TEST_CASE("parse_weeks 正常 JSON 解析周次列表", "[JsonParser]") {
    const char *json = R"([
      {"serialNumber":1,"name":"第1周","startDate":"2026-02-23","endDate":"2026-03-01","isCurrent":false},
      {"serialNumber":8,"name":"第8周","startDate":"2026-04-13","endDate":"2026-04-19","isCurrent":true}
    ])";

    auto result = um::Parser::parse_weeks(json);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 2);
    CHECK((*result)[0].serial_number == 1);
    CHECK((*result)[0].name == "第1周");
    CHECK((*result)[0].is_current == false);
    CHECK((*result)[1].serial_number == 8);
    CHECK((*result)[1].is_current == true);
}

TEST_CASE("parse_weeks 非法 JSON 返回 ParseError", "[JsonParser]") {
    auto result = um::Parser::parse_weeks("{not an array}");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
}
