#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

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

class VenueReservationLoginFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin") {
            response.status_code = 302;
            response.headers["Location"] = "/venue-zhjs-server/sso/landing";
        } else if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/landing") {
            response.headers["Set-Cookie"] = "JSESSIONID=session-1; Path=/\nsso_buaa_zhjs_token=sso-token-1; Path=/; HttpOnly";
            response.body = R"JSON({"code":200,"data":{}})JSON";
        } else if (request.url == "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fcgyy.buaa.edu.cn%2Fvenue-zhjs-server%2Fsso%2FmanageLogin") {
            response.status_code = 302;
            response.headers["Location"] = "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin";
        } else if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/api/login") {
            auto token = request.headers.find("Sso-Token");
            REQUIRE(token != request.headers.end());
            CHECK(token->second == "sso-token-1");
            response.body = R"JSON({"code":200,"data":{"token":{"access_token":"access-token-1"}}})JSON";
        } else if (request.url.find("https://cgyy.buaa.edu.cn/venue-zhjs-server/api/orders/mine?") == 0) {
            auto auth = request.headers.find("cgAuthorization");
            REQUIRE(auth != request.headers.end());
            CHECK(auth->second == "access-token-1");
            response.body = R"JSON({"code":200,"data":{"content":[{"id":"order-1","theme":"组会","status":"reserved","reservationDate":"2026-05-15"}]}})JSON";
        } else {
            response.status_code = 500;
            response.body = R"JSON({"code":500,"message":"unexpected request"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
};

} // namespace

TEST_CASE("VenueReservationService 跟随 SSO 重定向获取 token", "[service][cgyy]") {
    VenueReservationLoginFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::VenueReservationService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.orders();

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "order-1");
    CHECK(http_client.request_counts["https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/landing"] == 1);
}

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
