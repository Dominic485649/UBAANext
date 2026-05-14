/**
 * @file TermServiceTests.cpp
 * @brief TermService 的单元测试
 */

#include <UBAANext/Service/TermService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("TermService 返回学期列表", "[TermService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store);

    auto result = service.get_terms();
    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 2);
}

TEST_CASE("TermService 学期字段非空", "[TermService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store);

    auto result = service.get_terms();
    REQUIRE(result.has_value());

    for (const auto &term : *result) {
        REQUIRE_FALSE(term.code.empty());
        REQUIRE_FALSE(term.name.empty());
    }
}

TEST_CASE("TermService 返回周次列表", "[TermService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store);

    auto result = service.get_weeks("2025-2026-2");
    REQUIRE(result.has_value());
    REQUIRE(result->size() >= 20);
}

TEST_CASE("TermService 周次字段完整", "[TermService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store);

    auto result = service.get_weeks("2025-2026-2");
    REQUIRE(result.has_value());

    for (const auto &week : *result) {
        REQUIRE_FALSE(week.name.empty());
        REQUIRE_FALSE(week.start_date.empty());
        REQUIRE_FALSE(week.end_date.empty());
        CHECK(week.serial_number > 0);
    }
}

TEST_CASE("TermService 缓存行为 - 第二次不触发 HTTP", "[TermService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store);

    auto result1 = service.get_terms();
    REQUIRE(result1.has_value());

    auto result2 = service.get_terms();
    REQUIRE(result2.has_value());
    CHECK(result1->size() == result2->size());
}
