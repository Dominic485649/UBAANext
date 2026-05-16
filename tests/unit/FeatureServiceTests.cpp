#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

namespace {

class FeatureFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin") {
            response.headers["Set-Cookie"] = "sso_buaa_zhjs_token=sso-token-1; Path=/; HttpOnly";
            response.body = R"JSON({"code":200,"data":{}})JSON";
        } else if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/api/login") {
            response.body = R"JSON({"code":200,"data":{"token":{"access_token":"access-token-1"}}})JSON";
        } else if (request.url.find("https://cgyy.buaa.edu.cn/venue-zhjs-server/api/orders/lock/code?") == 0) {
            ++lock_code_requests;
            response.body = R"JSON({"code":200,"data":{"code":"123456"}})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"code":404,"message":"unexpected request"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
    int lock_code_requests = 0;
};

} // namespace

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

TEST_CASE("FeatureService 未知 mock 功能不伪造成功", "[service][feature]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Mock);

    auto result = service.list("unknown", "list");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NotImplemented);
}

TEST_CASE("FeatureService 真实门锁码不要求业务 ID", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.show("cgyy", "lock-code", "");

    REQUIRE(result);
    CHECK(result->id == "lock-code");
    CHECK(http_client.lock_code_requests == 1);
}
