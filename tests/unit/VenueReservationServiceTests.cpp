#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

class VenueReservationFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &) override {
        ++requests;
        UBAANext::HttpResponse response;
        response.status_code = 500;
        response.body = R"JSON({"code":500,"message":"unexpected request"})JSON";
        return response;
    }

    int requests = 0;
};

UBAANext::VenueReservationService make_service(VenueReservationFixtureHttpClient &http_client, UBAANext::MemoryCacheStore &cache) {
    return UBAANext::VenueReservationService(http_client, cache, UBAANext::ConnectionMode::Direct);
}

} // namespace

TEST_CASE("VenueReservationService 预约拒绝非数字场地 ID", "[service][cgyy]") {
    VenueReservationFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_service(http_client, cache);

    auto result = service.reserve("site-1", "space-x", "2026-05-15", "1001", "3", "组会", "13800000000", "张三", "captcha", "token");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.requests == 0);
}

TEST_CASE("VenueReservationService 预约拒绝非数字时段 ID", "[service][cgyy]") {
    VenueReservationFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_service(http_client, cache);

    auto result = service.reserve("site-1", "2001", "2026-05-15", "time-x", "3", "组会", "13800000000", "张三", "captcha", "token");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.requests == 0);
}
