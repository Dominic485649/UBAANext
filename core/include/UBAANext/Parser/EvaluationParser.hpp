#pragma once

#include <UBAANext/Model/Evaluation.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

std::vector<Model::EvaluationTask> parse_evaluation_required_reviews(
    const nlohmann::json &courses,
    const std::string &task_id,
    const std::string &questionnaire_id,
    const std::string &pattern_id,
    const std::string &term_code,
    const std::string &status);

} // namespace Parser
} // namespace UBAANext
