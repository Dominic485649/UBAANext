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
#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Service/TermService.hpp>
#include <UBAANext/Service/TodoService.hpp>
#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Service/YgdkService.hpp>

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
    [[nodiscard]] UBAANext::SpocService create_spoc_service();
    [[nodiscard]] UBAANext::SigninService create_signin_service();
    [[nodiscard]] UBAANext::YgdkService create_ygdk_service();
    [[nodiscard]] UBAANext::EvaluationService create_evaluation_service();
    [[nodiscard]] UBAANext::BykcService create_bykc_service();
    [[nodiscard]] UBAANext::VenueReservationService create_venue_reservation_service();
    [[nodiscard]] UBAANext::LibrarySeatService create_library_seat_service();
    [[nodiscard]] UBAANext::TodoService create_todo_service();
    [[nodiscard]] UBAANext::FeatureService create_feature_service();

    [[nodiscard]] UBAANext::IHttpClient &http_client();
    [[nodiscard]] const AppContext &context() const { return m_ctx; }

private:
    AppContext &m_ctx;
};

} // namespace UBAANextCli
