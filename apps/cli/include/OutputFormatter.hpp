/**
 * @file OutputFormatter.hpp
 * @brief 统一输出格式化器
 *
 * 提供人类可读输出和 JSON 输出两种模式。
 * 所有命令的输出通过此接口统一格式化。
 */
#pragma once

#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Model/Classroom.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Auth/AuthService.hpp>

#include <string>
#include <vector>

namespace UBAANextCli {

/**
 * @brief 输出格式化器
 *
 * 根据 json_mode 标志选择人类可读或 JSON 输出格式。
 * JSON 输出格式遵循 CLI 命令 API 合同：
 *   成功: {"ok": true, "data": {...}, "error": null}
 *   失败: {"ok": false, "data": null, "error": {"code": "...", "message": "..."}}
 */
class OutputFormatter {
public:
    explicit OutputFormatter(bool json_mode);

    void print_courses(const std::vector<UBAANext::Model::Course> &courses, int week = 0);
    void print_exams(const std::vector<UBAANext::Model::Exam> &exams);
    void print_grades(const std::vector<UBAANext::Model::Grade> &grades);
    void print_classrooms(const UBAANext::Model::ClassroomQueryResult &qr);
    void print_terms(const std::vector<UBAANext::Model::Term> &terms);
    void print_weeks(const std::vector<UBAANext::Model::Week> &weeks);
    void print_account(const UBAANext::Model::Account &account);
    void print_login_result(const std::string &message, const UBAANext::Model::Account &account);
    void print_records(const std::string &key, const std::vector<UBAANext::Model::FeatureRecord> &records);
    void print_record(const std::string &key, const UBAANext::Model::FeatureRecord &record);
    void print_mutation(const UBAANext::Model::MutationResult &result);
    void print_version(const std::string &version);
    void print_error(const UBAANext::Error &error);
    void print_message(const std::string &msg);

    [[nodiscard]] bool is_json() const { return m_json; }

private:
    bool m_json;
};

} // namespace UBAANextCli
