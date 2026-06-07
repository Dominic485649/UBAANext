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
    CHECK(records[0].evaluation_type_id == "2");
    CHECK(records[0].allow_all == "1");
    CHECK(records[0].evaluated_count == 0);
    CHECK(records[0].required_count == 1);

    CHECK(records[1].id == "task-1_questionnaire-1_ME101_teacher-2");
    CHECK(records[1].evaluated_count == 1);
    CHECK(records[1].required_count == 2);
}

TEST_CASE("parse_evaluation_required_reviews 保留提交所需字段", "[EvaluationParser]") {
    auto records = um::Parser::parse_evaluation_required_reviews(
        nlohmann::json::array({{{"kcdm", "CS201"}, {"kcmc", "算法设计"}, {"bpdm", "teacher-9"}, {"bpmc", "赵老师"}, {"pjrdm", "student-1"}, {"pjrmc", "学生"}, {"rwh", "task-no"}, {"xn", "2025"}, {"xq", "2"}, {"pjlxid", "3"}, {"sfksqbpj", "0"}, {"yxsfktjst", "1"}}}),
        "task-2",
        "questionnaire-2",
        "pattern-2",
        "20252026",
        "pending");

    REQUIRE(records.size() == 1);
    CHECK(records[0].evaluator_code == "student-1");
    CHECK(records[0].evaluator_name == "学生");
    CHECK(records[0].assignment_no == "task-no");
    CHECK(records[0].year == "2025");
    CHECK(records[0].semester == "2");
    CHECK(records[0].evaluation_type_id == "3");
    CHECK(records[0].allow_all == "0");
    CHECK(records[0].department_submit_status == "1");
}

TEST_CASE("parse_evaluation_required_reviews 非数组返回空列表", "[EvaluationParser]") {
    auto records = um::Parser::parse_evaluation_required_reviews(
        nlohmann::json::object(), "task", "wj", "1", "term", "evaluated");

    CHECK(records.empty());
}

TEST_CASE("parse_evaluation_form 解析题目选项和提交上下文", "[EvaluationParser]") {
    um::Model::EvaluationTask task;
    task.id = "task-1_questionnaire-1_CS101_teacher-1";
    task.title = "程序设计";
    task.status = "pending";
    task.task_id = "task-1";
    task.questionnaire_id = "questionnaire-1";
    task.course_code = "CS101";
    task.teacher_code = "teacher-1";
    task.term_code = "20252026";
    task.pattern_id = "1";

    auto form = um::Parser::parse_evaluation_form(nlohmann::json::array({{
        {"pjxtWjWjbReturnEntity", {{"wjzblist", nlohmann::json::array({{{"tklist", nlohmann::json::array({
            {{"tmid", "q1"}, {"tgmc", "整体评价"}, {"tmlx", "1"}, {"tmxxlist", nlohmann::json::array({{{"tmxxid", "a1"}, {"xxmc", "优秀"}, {"xxfz", 90}}, {{"tmxxid", "a2"}, {"xxfz", "95"}}})}},
            {{"tmid", "q2"}, {"tgmc", "建议"}, {"tmlx", "6"}, {"tmxxlist", nlohmann::json::array({{{"tmxxid", "text-1"}}})}},
        })}}})}}},
        {"pjxtPjjgPjjgckb", nlohmann::json::array({{{"pjid", "pj-1"}, {"pjrjsdm", "role-1"}, {"wjssrwid", "rel-1"}, {"pjrdm", "student-1"}}})},
        {"pjmap", {{"PJJGXXBM", "detail-map"}, {"RWID", "task-1"}, {"PJJGBM", "result-map"}}},
    }}), task);

    CHECK(form.form_result_id == "pj-1");
    CHECK(form.evaluator_role_id == "role-1");
    CHECK(form.evaluated_relation_id == "rel-1");
    CHECK(form.result_map["PJJGXXBM"] == "detail-map");
    REQUIRE(form.questions.size() == 2);
    CHECK(form.questions[0].id == "q1");
    CHECK(form.questions[0].is_choice);
    REQUIRE(form.questions[0].choices.size() == 2);
    CHECK(form.questions[0].choices[1].id == "a2");
    CHECK(form.questions[0].choices[1].score == 95.0);
    CHECK_FALSE(form.questions[1].is_choice);
}
