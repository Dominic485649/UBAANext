#include <UBAANext/Protocol/ScoreSession.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/TermService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("P1 真实周课表拒绝隐式默认学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_week_courses(8);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实周课表拒绝无效周次", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_week_courses(0, "2025-2026-2");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实考试查询拒绝隐式默认学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ExamService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_exams();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实周次查询拒绝空学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_weeks("");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实成绩查询拒绝空学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::GradeService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.list_grades("");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 Score session 识别登录失效响应", "[p1][real-readonly]") {
    um::HttpResponse response;
    response.status_code = 200;
    response.body = R"({"message":"请重新登录"})";

    CHECK(um::Protocol::Score::is_session_expired_response(response));
}
