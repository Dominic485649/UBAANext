#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Judge.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::JudgeCourse> parse_judge_courses_html(const std::string &html);
std::vector<Model::JudgeAssignmentSummary> parse_judge_assignments_html(const std::string &html,
                                                                        const Model::JudgeCourse &course);
Result<Model::JudgeAssignmentDetail> parse_judge_assignment_detail_html(const std::string &html,
                                                                        const Model::JudgeAssignmentSummary &summary);

} // namespace Parser
} // namespace UBAANext
