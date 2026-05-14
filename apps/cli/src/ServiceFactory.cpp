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

UBAANext::FeatureService ServiceFactory::create_feature_service() {
    return UBAANext::FeatureService(*m_ctx.http, *m_ctx.cache, m_ctx.conn_mode);
}

} // namespace UBAANextCli
