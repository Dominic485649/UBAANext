/**
 * @file ServiceFactory.hpp
 * @brief 服务工厂，集中创建 Core Service 实例
 *
 * 根据 AppContext 中的运行模式创建对应的 Service，
 * 命令处理函数不直接 new/构造 Service。
 */
#pragma once

#include "AppContext.hpp"

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Service/ClassroomService.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/TermService.hpp>

namespace UBAANextCli {

/**
 * @brief 服务工厂
 *
 * 持有 AppContext 引用，按需创建 Service 实例。
 * Service 生命周期由调用方管理（通常为栈上局部变量）。
 */
class ServiceFactory {
public:
    explicit ServiceFactory(AppContext &ctx);

    [[nodiscard]] UBAANext::AuthService create_auth_service();
    [[nodiscard]] UBAANext::CourseService create_course_service();
    [[nodiscard]] UBAANext::ExamService create_exam_service();
    [[nodiscard]] UBAANext::ClassroomService create_classroom_service();
    [[nodiscard]] UBAANext::TermService create_term_service();
    [[nodiscard]] UBAANext::GradeService create_grade_service();
    [[nodiscard]] UBAANext::JudgeService create_judge_service();
    [[nodiscard]] UBAANext::FeatureService create_feature_service();

    [[nodiscard]] UBAANext::IHttpClient &http_client() { return *m_ctx.http; }
    [[nodiscard]] const AppContext &context() const { return m_ctx; }

private:
    AppContext &m_ctx;
};

} // namespace UBAANextCli
