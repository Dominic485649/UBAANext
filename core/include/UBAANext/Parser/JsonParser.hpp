/**
 * @file JsonParser.hpp
 * @brief JSON 响应解析器
 *
 * 声明 UBAA API JSON 响应的解析函数。
 * 使用 nlohmann/json 将 JSON 字符串解析为对应的 Model 结构体。
 */
#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Classroom.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>

#include <string>
#include <vector>

namespace UBAANext::Parser {

/**
 * @brief 从 JSON 响应体中解析课程列表
 * @param json 来自 API 的原始 JSON 字符串（JSON 数组）
 * @return 课程列表，或 ParseError
 */
Result<std::vector<Model::Course>> parse_courses(const std::string &json);

/**
 * @brief 从 JSON 响应体中解析考试列表
 * @param json 来自 API 的原始 JSON 字符串（JSON 数组）
 * @return 考试列表，或 ParseError
 */
Result<std::vector<Model::Exam>> parse_exams(const std::string &json);

/**
 * @brief 从 JSON 响应体中解析教室可用性
 * @param json 来自 API 的原始 JSON 字符串（JSON 对象，含 buildings 字段）
 * @return 教室可用性数据，或 ParseError
 */
Result<Model::ClassroomQueryResult> parse_classrooms(const std::string &json);

/**
 * @brief 从 JSON 响应体中解析学期列表
 * @param json 来自 API 的原始 JSON 字符串（JSON 数组）
 * @return 学期列表，或 ParseError
 */
Result<std::vector<Model::Term>> parse_terms(const std::string &json);

/**
 * @brief 从 JSON 响应体中解析教学周列表
 * @param json 来自 API 的原始 JSON 字符串（JSON 数组）
 * @return 教学周列表，或 ParseError
 */
Result<std::vector<Model::Week>> parse_weeks(const std::string &json);

} // namespace UBAANext::Parser
