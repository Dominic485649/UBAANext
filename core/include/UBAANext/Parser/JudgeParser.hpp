#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Judge.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** Sensitive output: parses raw XiJi course HTML; HTML must not be logged verbatim. */
std::vector<Model::JudgeCourse> parse_judge_courses_html(const std::string &html);
/** PartiallyMigrated parser entry: parses XiJi assignment list HTML; DOM drift remains possible. */
std::vector<Model::JudgeAssignmentSummary> parse_judge_assignments_html(const std::string &html,
                                                                        const Model::JudgeCourse &course);
/** Sensitive output: parses XiJi assignment detail HTML and returns ParseError on unsupported detail shape. */
Result<Model::JudgeAssignmentDetail> parse_judge_assignment_detail_html(const std::string &html,
                                                                        const Model::JudgeAssignmentSummary &summary);

} // namespace Parser
} // namespace UBAANext
