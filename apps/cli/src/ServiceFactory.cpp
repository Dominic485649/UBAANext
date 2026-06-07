/**
 * @file ServiceFactory.cpp
 * @brief 服务工厂实现
 */

#include "ServiceFactory.hpp"

#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/SecureStore.hpp>

#include <optional>
#include <utility>

namespace UBAANextCli {

ServiceFactory::ServiceFactory(AppContext &ctx) : m_ctx(ctx) {}

UBAANext::IHttpClient &ServiceFactory::http_client() {
    return m_ctx.network_stack ? m_ctx.network_stack->http_client() : *m_ctx.http;
}

UBAANext::ICryptoProvider &ServiceFactory::crypto_provider() {
    return m_ctx.crypto ? *m_ctx.crypto : UBAANext::default_crypto_provider();
}

UBAANext::AuthService ServiceFactory::create_auth_service() {
    UBAANext::AuthService auth(http_client(), *m_ctx.store);
    auth.set_connection_mode(m_ctx.conn_mode);
    return auth;
}

UBAANext::CourseService ServiceFactory::create_course_service() {
    return UBAANext::CourseService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::ExamService ServiceFactory::create_exam_service() {
    return UBAANext::ExamService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::ClassroomService ServiceFactory::create_classroom_service() {
    return UBAANext::ClassroomService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::TermService ServiceFactory::create_term_service() {
    return UBAANext::TermService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::GradeService ServiceFactory::create_grade_service() {
    return UBAANext::GradeService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::JudgeService ServiceFactory::create_judge_service() {
    return UBAANext::JudgeService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::SpocService ServiceFactory::create_spoc_service() {
    return UBAANext::SpocService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::SpocService ServiceFactory::create_spoc_write_service(bool confirmed, const std::string &operation) {
    auto service = create_spoc_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::SrsService ServiceFactory::create_srs_service() {
    return UBAANext::SrsService(http_client(), m_ctx.network_stack ? &m_ctx.network_stack->cookie_store() : nullptr, *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::SrsService ServiceFactory::create_srs_write_service(bool confirmed, const std::string &operation) {
    auto service = create_srs_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::SigninService ServiceFactory::create_signin_service() {
    auto auth = create_auth_service();
    auto account = auth.restore_session();
    std::string student_id = account ? account->student_id : std::string{};
    return UBAANext::SigninService(http_client(), *m_ctx.cache, m_ctx.conn_mode, std::move(student_id));
}

UBAANext::SigninService ServiceFactory::create_signin_write_service(bool confirmed, const std::string &operation) {
    auto service = create_signin_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::YgdkService ServiceFactory::create_ygdk_service() {
    return UBAANext::YgdkService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::YgdkService ServiceFactory::create_ygdk_write_service(bool confirmed, const std::string &operation) {
    auto service = create_ygdk_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::EvaluationService ServiceFactory::create_evaluation_service() {
    return UBAANext::EvaluationService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::EvaluationService ServiceFactory::create_evaluation_write_service(bool confirmed, const std::string &operation) {
    auto service = create_evaluation_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::BykcService ServiceFactory::create_bykc_service() {
    return UBAANext::BykcService(http_client(), *m_ctx.cache, m_ctx.conn_mode, crypto_provider());
}

UBAANext::BykcService ServiceFactory::create_bykc_write_service(bool confirmed, const std::string &operation) {
    auto service = create_bykc_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::VenueReservationService ServiceFactory::create_venue_reservation_service() {
    return UBAANext::VenueReservationService(http_client(), m_ctx.network_stack ? &m_ctx.network_stack->cookie_store() : nullptr, *m_ctx.cache, m_ctx.conn_mode, crypto_provider());
}

UBAANext::VenueReservationService ServiceFactory::create_venue_reservation_write_service(bool confirmed, const std::string &operation) {
    auto service = create_venue_reservation_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::LibrarySeatService ServiceFactory::create_library_seat_service() {
    return UBAANext::LibrarySeatService(http_client(), *m_ctx.cache, m_ctx.conn_mode, crypto_provider());
}

UBAANext::LibrarySeatService ServiceFactory::create_library_seat_write_service(bool confirmed, const std::string &operation) {
    auto service = create_library_seat_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::LiveService ServiceFactory::create_live_service() {
    return UBAANext::LiveService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::CloudService ServiceFactory::create_cloud_service() {
    std::optional<UBAANext::CloudLoginCredentials> credentials;
    if (m_ctx.store) {
        const auto username = m_ctx.store->get_string("login.username");
        const auto password = m_ctx.store->get_string("login.password");
        if (username && password && !username->empty() && !password->empty()) {
            credentials = UBAANext::CloudLoginCredentials{*username, *password};
        }
    }
    return UBAANext::CloudService(http_client(), m_ctx.network_stack ? &m_ctx.network_stack->cookie_store() : nullptr, *m_ctx.cache, m_ctx.conn_mode, crypto_provider(), std::move(credentials));
}

UBAANext::CloudService ServiceFactory::create_cloud_write_service(bool confirmed, const std::string &operation) {
    auto service = create_cloud_service();
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::WifiService ServiceFactory::create_wifi_write_service(bool confirmed, const std::string &operation, UBAANext::Model::WifiCredentials credentials) {
    if (credentials.username.empty() && m_ctx.store) {
        const auto username = m_ctx.store->get_string("login.username");
        if (username) credentials.username = *username;
    }
    if (credentials.password.empty() && m_ctx.store) {
        const auto password = m_ctx.store->get_string("login.password");
        if (password) credentials.password = *password;
    }
    auto service = UBAANext::WifiService(http_client(), m_ctx.network_environment.get(), crypto_provider(), std::move(credentials));
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(m_ctx.capabilities, operation, confirmed));
    return service;
}

UBAANext::TodoService ServiceFactory::create_todo_service() {
    return UBAANext::TodoService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

UBAANext::FeatureService ServiceFactory::create_feature_service() {
    return UBAANext::FeatureService(http_client(), *m_ctx.cache, m_ctx.conn_mode);
}

} // namespace UBAANextCli
