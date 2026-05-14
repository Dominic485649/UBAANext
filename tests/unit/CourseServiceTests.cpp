/**
 * @file CourseServiceTests.cpp
 * @brief CourseService 类的单元测试
 */

#include <UBAANext/Service/CourseService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("CourseService 返回课程", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store);

    auto result = service.get_today_courses();
    REQUIRE(result.has_value());
    REQUIRE(result->size() > 0);
}

TEST_CASE("CourseService 课程字段非空", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store);

    auto result = service.get_today_courses();
    REQUIRE(result.has_value());

    for (const auto &course : *result) {
        REQUIRE_FALSE(course.name.empty());
        REQUIRE_FALSE(course.classroom.empty());
    }
}

TEST_CASE("CourseService 返回至少 3 门课程", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store);

    auto result = service.get_today_courses();
    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 3);
}

TEST_CASE("CourseService get_week_courses 按周次过滤", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store);

    auto result = service.get_week_courses(8);
    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 3);

    for (const auto &c : *result) {
        CHECK(8 >= c.week_start);
        CHECK(8 <= c.week_end);
    }
}

TEST_CASE("CourseService 缓存行为 - 第二次命中缓存", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store);

    auto result1 = service.get_today_courses();
    REQUIRE(result1.has_value());

    auto result2 = service.get_today_courses();
    REQUIRE(result2.has_value());
    CHECK(result1->size() == result2->size());
}

TEST_CASE("CourseService 网络失败返回 NetworkError", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    http_client.set_network_error("/schedule/today", "连接超时");
    um::CourseService service(http_client, cache_store);

    auto result = service.get_today_courses();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::NetworkError);
}

TEST_CASE("CourseService HTTP 500 返回 NetworkError", "[CourseService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    http_client.set_http_error("/schedule/today", 500, "Internal Server Error");
    um::CourseService service(http_client, cache_store);

    auto result = service.get_today_courses();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::NetworkError);
}
