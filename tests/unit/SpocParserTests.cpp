#include <UBAANext/Parser/SpocParser.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <map>
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

TEST_CASE("parse_spoc_assignments_page 解析分页作业并补齐课程信息", "[SpocParser]") {
    std::map<std::string, std::pair<std::string, std::string>> courses{
        {"course-1", {"智能制造导论", "张老师"}},
        {"course-2", {"课程表名称不应覆盖列表名称", "李老师"}},
    };

    auto records = um::Parser::parse_spoc_assignments_page(
        load_json_fixture("spoc/assignments_page.json"), courses, "2025-2026-2", "2025-2026学年第二学期");

    REQUIRE(records.size() == 3);
    CHECK(records[0].id == "spoc-1");
    CHECK(records[0].course_id == "course-1");
    CHECK(records[0].course_name == "智能制造导论");
    CHECK(records[0].teacher == "张老师");
    CHECK(records[0].title == "第一章作业");
    CHECK(records[0].status == "submitted");
    CHECK(records[0].start_time == "2026-03-01 08:00:00");
    CHECK(records[0].due_time == "2026-03-08 23:59:00");
    CHECK(records[0].score == "100");
    CHECK(records[0].term_code == "2025-2026-2");
    CHECK(records[0].term_name == "2025-2026学年第二学期");
    CHECK(records[0].submission_status == "已提交");

    CHECK(records[1].course_name == "工程训练");
    CHECK(records[1].teacher == "李老师");
    CHECK(records[1].status == "unsubmitted");
    CHECK(records[1].score == "50");
    CHECK(records[2].id == "spoc-3");
    CHECK(records[2].status == "expired");
    CHECK(records[2].submission_status == "已过期");
}

TEST_CASE("parse_spoc_assignments_page 规范化时间分数和未知状态", "[SpocParser]") {
    auto records = um::Parser::parse_spoc_assignments_page(
        nlohmann::json{{"list", nlohmann::json::array({{{"zyid", "spoc-iso"}, {"sskcid", "course-1"}, {"zymc", "ISO 作业"}, {"tjzt", "9"}, {"zykssj", "2026-03-24T08:00:00.000+00:00"}, {"zyjzsj", "2026-03-31T15:59:59.000+00:00"}, {"mf", "满分:100"}}})}},
        {}, "2025-2026-2", "2025-2026学年第二学期");

    REQUIRE(records.size() == 1);
    CHECK(records[0].status == "unknown");
    CHECK(records[0].start_time == "2026-03-24 16:00:00");
    CHECK(records[0].due_time == "2026-03-31 23:59:59");
    CHECK(records[0].score == "100");
}

TEST_CASE("parse_spoc_assignments_page 规范化 Z 时区时间", "[SpocParser]") {
    auto records = um::Parser::parse_spoc_assignments_page(
        nlohmann::json{{"list", nlohmann::json::array({{{"zyid", "spoc-z"}, {"zymc", "Z 作业"}, {"zykssj", "2026-03-24T08:00:00.000Z"}}})}},
        {}, "2025-2026-2", "2025-2026学年第二学期");

    REQUIRE(records.size() == 1);
    CHECK(records[0].start_time == "2026-03-24 16:00:00");
}

TEST_CASE("parse_spoc_assignments_page 异常 ISO 时间保持原值", "[SpocParser]") {
    auto records = um::Parser::parse_spoc_assignments_page(
        nlohmann::json{{"list", nlohmann::json::array({{{"zyid", "spoc-bad-time"}, {"zymc", "异常时间"}, {"zykssj", "999999999999999999999999999999-03-24T08:00:00.000Z"}}})}},
        {}, "2025-2026-2", "2025-2026学年第二学期");

    REQUIRE(records.size() == 1);
    CHECK(records[0].start_time == "999999999999999999999999999999-03-24 08:00:00");
}

TEST_CASE("parse_spoc_assignment_detail 解析详情和提交状态", "[SpocParser]") {
    auto detail_json = load_json_fixture("spoc/detail.json");
    auto submission_json = load_json_fixture("spoc/submission.json");

    auto detail = um::Parser::parse_spoc_assignment_detail(detail_json, &submission_json, "spoc-1");

    CHECK(detail.id == "spoc-1");
    CHECK(detail.course_id == "course-1");
    CHECK(detail.title == "第一章作业");
    CHECK(detail.status == "submitted");
    CHECK(detail.start_time == "2026-03-01 08:00:00");
    CHECK(detail.due_time == "2026-03-08 23:59:00");
    CHECK(detail.score == "100");
    CHECK(detail.content == "阅读材料 并完成练习 提交 PDF");
    CHECK(detail.submission_status == "1");
    CHECK(detail.submitted_at == "2026-03-04 12:30:00");
}

TEST_CASE("parse_spoc_assignment_detail 缺少提交记录时状态为 unknown", "[SpocParser]") {
    auto detail = um::Parser::parse_spoc_assignment_detail(load_json_fixture("spoc/detail.json"), nullptr, "spoc-1");

    CHECK(detail.status == "unknown");
    CHECK(detail.submission_status.empty());
    CHECK(detail.submitted_at.empty());
}

TEST_CASE("parse_spoc_assignment_detail 规范化提交时间和未知状态", "[SpocParser]") {
    auto detail_json = nlohmann::json{{"sskcid", "course-1"}, {"zymc", "实验作业"}, {"zykssj", "2026-03-24T08:00:00.000+00:00"}, {"zyjzsj", "2026-03-31T15:59:59.000+00:00"}, {"zyfs", "满分:98.5"}, {"zynr", "<p>&nbsp;提交报告</p>"}};
    auto submission_json = nlohmann::json{{"tjzt", "9"}, {"tjsj", "2026-03-25T02:30:00.000+00:00"}};

    auto detail = um::Parser::parse_spoc_assignment_detail(detail_json, &submission_json, "spoc-iso");

    CHECK(detail.status == "unknown");
    CHECK(detail.start_time == "2026-03-24 16:00:00");
    CHECK(detail.due_time == "2026-03-31 23:59:59");
    CHECK(detail.score == "98.5");
    CHECK(detail.content == "提交报告");
    CHECK(detail.submitted_at == "2026-03-25 10:30:00");
}
