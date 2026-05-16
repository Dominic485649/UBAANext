#include <UBAANext/Parser/JudgeParser.hpp>

#include <catch2/catch_test_macros.hpp>

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

} // namespace

TEST_CASE("parse_judge_courses_html 解析课程并去重", "[JudgeParser]") {
    auto courses = um::Parser::parse_judge_courses_html(load_fixture("judge/courses.html"));

    REQUIRE(courses.size() == 2);
    CHECK(courses[0].id == "1001");
    CHECK(courses[0].name == "程序设计基础");
    CHECK(courses[1].id == "1002");
    CHECK(courses[1].name == "数据结构");
}

TEST_CASE("parse_judge_assignments_html 解析作业并过滤题面链接", "[JudgeParser]") {
    um::Model::JudgeCourse course{"1001", "程序设计基础"};
    auto assignments = um::Parser::parse_judge_assignments_html(load_fixture("judge/assignments.html"), course);

    REQUIRE(assignments.size() == 3);
    CHECK(assignments[0].id == "9001");
    CHECK(assignments[0].course_id == "1001");
    CHECK(assignments[0].course_name == "程序设计基础");
    CHECK(assignments[0].title == "第一周练习 A+B");
    CHECK(assignments[0].status == "unsubmitted");
    CHECK(assignments[1].id == "9002");
    CHECK(assignments[1].status == "expired");
    CHECK(assignments[2].id == "9003");
    CHECK(assignments[2].status == "submitted");
}

TEST_CASE("parse_judge_assignment_detail_html 解析详情与提交状态", "[JudgeParser]") {
    um::Model::JudgeAssignmentSummary summary;
    summary.id = "9001";
    summary.course_id = "1001";
    summary.course_name = "程序设计基础";
    summary.title = "第一周练习 A+B";
    summary.status = "available";
    auto detail = um::Parser::parse_judge_assignment_detail_html(load_fixture("judge/detail.html"), summary);

    REQUIRE(detail.has_value());
    CHECK(detail->id == "9001");
    CHECK(detail->course_id == "1001");
    CHECK(detail->course_name == "程序设计基础");
    CHECK(detail->title == "第一周练习 A+B");
    CHECK(detail->start_time == "2026-03-01 08:00:00");
    CHECK(detail->due_time == "2026-03-08 23:59:00");
    CHECK(detail->max_score == "100");
    CHECK(detail->my_score == "50");
    CHECK(detail->total_problems == 3);
    CHECK(detail->submitted_count == 2);
    CHECK(detail->status == "partial");
    CHECK(detail->status_text == "进行中(2/3)");
}
