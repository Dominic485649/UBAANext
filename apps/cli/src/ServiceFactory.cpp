/**
 * @file ServiceFactory.cpp
 * @brief 服务工厂实现
 */

#include "ServiceFactory.hpp"

namespace UBAANextCli {

ServiceFactory::ServiceFactory(AppContext &ctx) : m_ctx(ctx) {}

UBAANext::AuthService ServiceFactory::create_auth_service() {
    UBAANext::AuthService auth(*m_ctx.http, *m_ctx.store);
    auth.set_connection_mode(m_ctx.conn_mode);
    return auth;
}

UBAANext::CourseService ServiceFactory::create_course_service() {
    return UBAANext::CourseService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::ExamService ServiceFactory::create_exam_service() {
    return UBAANext::ExamService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::ClassroomService ServiceFactory::create_classroom_service() {
    return UBAANext::ClassroomService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::TermService ServiceFactory::create_term_service() {
    return UBAANext::TermService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::GradeService ServiceFactory::create_grade_service() {
    return UBAANext::GradeService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::JudgeService ServiceFactory::create_judge_service() {
    return UBAANext::JudgeService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::SpocService ServiceFactory::create_spoc_service() {
    return UBAANext::SpocService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::SigninService ServiceFactory::create_signin_service() {
    return UBAANext::SigninService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::YgdkService ServiceFactory::create_ygdk_service() {
    return UBAANext::YgdkService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::EvaluationService ServiceFactory::create_evaluation_service() {
    return UBAANext::EvaluationService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::BykcService ServiceFactory::create_bykc_service() {
    return UBAANext::BykcService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::VenueReservationService ServiceFactory::create_venue_reservation_service() {
    return UBAANext::VenueReservationService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::LibrarySeatService ServiceFactory::create_library_seat_service() {
    return UBAANext::LibrarySeatService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::TodoService ServiceFactory::create_todo_service() {
    return UBAANext::TodoService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::FeatureService ServiceFactory::create_feature_service() {
    return UBAANext::FeatureService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

} // namespace UBAANextCli
