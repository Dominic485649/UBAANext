/**
 * @file ClassroomServiceTests.cpp
 * @brief ClassroomService 类的单元测试
 */

#include <UBAANext/Service/ClassroomService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("ClassroomService 返回教室", "[ClassroomService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ClassroomService service(http_client, cache_store);

    auto result = service.query_classrooms(1, "2026-05-12");
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->buildings.empty());
}

TEST_CASE("ClassroomService 教室包含空闲节次", "[ClassroomService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ClassroomService service(http_client, cache_store);

    auto result = service.query_classrooms(1, "2026-05-12");
    REQUIRE(result.has_value());

    for (const auto &[building, rooms] : result->buildings) {
        for (const auto &room : rooms) {
            REQUIRE_FALSE(room.name.empty());
            REQUIRE_FALSE(room.free_sections.empty());
        }
    }
}

TEST_CASE("ClassroomService 缓存行为", "[ClassroomService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ClassroomService service(http_client, cache_store);

    auto result1 = service.query_classrooms(1, "2026-05-12");
    REQUIRE(result1.has_value());

    auto result2 = service.query_classrooms(1, "2026-05-12");
    REQUIRE(result2.has_value());
    CHECK(result1->buildings.size() == result2->buildings.size());
}
