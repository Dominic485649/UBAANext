#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("FeatureService 明确公告真实协议未接入", "[service][feature]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list("announcement", "list");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NotImplemented);
    CHECK(result.error().message == "公告真实协议尚未接入");
}

TEST_CASE("FeatureService 公告 mock 保持可用", "[service][feature]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Mock);

    auto result = service.list("announcement", "list");

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "ann-1");
    CHECK((*result)[0].status == "published");
}
