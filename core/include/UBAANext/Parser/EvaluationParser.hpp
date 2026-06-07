#pragma once

#include <UBAANext/Model/Evaluation.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace UBAANext {
namespace Parser {

/** PartiallyMigrated parser entry: expands evaluation course/questionnaire JSON; questionnaire/session drift remains possible. */
std::vector<Model::EvaluationTask> parse_evaluation_required_reviews(
    const nlohmann::json &courses,
    const std::string &task_id,
    const std::string &questionnaire_id,
    const std::string &pattern_id,
    const std::string &term_code,
    const std::string &status);

/** Fully migrated parser entry: extracts questionnaire metadata, choices, and submit context. */
Model::EvaluationForm parse_evaluation_form(const nlohmann::json &topic,
                                            const Model::EvaluationTask &task);

} // namespace Parser
} // namespace UBAANext
