#include <UBAANext/Parser/EvaluationParser.hpp>

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

TEST_CASE("parse_evaluation_required_reviews 解析待评教课程", "[EvaluationParser]") {
    auto records = um::Parser::parse_evaluation_required_reviews(
        load_json_fixture("evaluation/required_reviews.json"),
        "task-1",
        "questionnaire-1",
        "pattern-1",
        "2025-2026-2",
        "pending");

    REQUIRE(records.size() == 2);
    CHECK(records[0].id == "task-1_questionnaire-1_CS101_teacher-1");
    CHECK(records[0].title == "程序设计");
    CHECK(records[0].status == "pending");
    CHECK(records[0].teacher == "陈老师");
    CHECK(records[0].task_id == "task-1");
    CHECK(records[0].questionnaire_id == "questionnaire-1");
    CHECK(records[0].course_code == "CS101");
    CHECK(records[0].teacher_code == "teacher-1");
    CHECK(records[0].term_code == "2025-2026-2");
    CHECK(records[0].pattern_id == "pattern-1");
    CHECK(records[0].evaluated_count == 0);
    CHECK(records[0].required_count == 1);

    CHECK(records[1].id == "task-1_questionnaire-1_ME101_teacher-2");
    CHECK(records[1].evaluated_count == 1);
    CHECK(records[1].required_count == 2);
}

TEST_CASE("parse_evaluation_required_reviews 非数组返回空列表", "[EvaluationParser]") {
    auto records = um::Parser::parse_evaluation_required_reviews(
        nlohmann::json::object(), "task", "wj", "1", "term", "evaluated");

    CHECK(records.empty());
}
