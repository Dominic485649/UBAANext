/**
 * @file ExamServiceTests.cpp
 * @brief ExamService 类的单元测试
 */

#include <UBAANext/Service/ExamService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("ExamService 返回考试", "[ExamService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ExamService service(http_client, cache_store);

    auto result = service.get_exams();
    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 3);
}

TEST_CASE("ExamService 考试包含必需字段", "[ExamService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ExamService service(http_client, cache_store);

    auto result = service.get_exams();
    REQUIRE(result.has_value());

    for (const auto &exam : *result) {
        REQUIRE_FALSE(exam.course_name.empty());
        REQUIRE_FALSE(exam.location.empty());
        REQUIRE_FALSE(exam.time_text.empty());
    }
}

TEST_CASE("ExamService 缓存行为", "[ExamService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ExamService service(http_client, cache_store);

    auto result1 = service.get_exams();
    REQUIRE(result1.has_value());

    auto result2 = service.get_exams();
    REQUIRE(result2.has_value());
    CHECK(result1->size() == result2->size());
}

TEST_CASE("ExamService 网络失败返回 NetworkError", "[ExamService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    http_client.set_network_error("/exam/list", "DNS 解析失败");
    um::ExamService service(http_client, cache_store);

    auto result = service.get_exams();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::NetworkError);
}

TEST_CASE("ExamService HTTP 401 返回 NetworkError", "[ExamService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    http_client.set_http_error("/exam/list", 401, "Unauthorized");
    um::ExamService service(http_client, cache_store);

    auto result = service.get_exams();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::NetworkError);
}
