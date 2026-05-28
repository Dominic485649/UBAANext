#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SigninService 默认写门控在网络请求前 fail closed", "[service][write-gate]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.perform_signin("course-1");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.request_count("https://uc.buaa.edu.cn/api/uc/userinfo") == 0);
}

TEST_CASE("WriteOperationGate 要求显式确认", "[service][write-gate]") {
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;

    auto result = UBAANext::require_write_operation(
        UBAANext::confirmed_write_operation(capabilities, "signin do", false));

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(result.error().message.find("signin do") != std::string::npos);
}

TEST_CASE("WriteOperationGate 在平台未开启写能力时 fail closed", "[service][write-gate]") {
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = false;

    auto result = UBAANext::require_write_operation(
        UBAANext::confirmed_write_operation(capabilities, "bykc select", true));

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    CHECK(result.error().message.find("bykc select") != std::string::npos);
}

TEST_CASE("WriteOperationGate 仅在确认且平台开启时放行", "[service][write-gate]") {
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;

    auto result = UBAANext::require_write_operation(
        UBAANext::confirmed_write_operation(capabilities, "libbook book", true));

    REQUIRE(result);
}
