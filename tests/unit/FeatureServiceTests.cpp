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
        } else if (request.url.find("https://cgyy.buaa.edu.cn/venue-zhjs-server/api/orders/mine?") == 0) {
            auto auth = request.headers.find("cgAuthorization");
            REQUIRE(auth != request.headers.end());
            CHECK(auth->second == "access-token-1");
            CHECK(request.url.find("page=0") != std::string::npos);
            CHECK(request.url.find("size=20") != std::string::npos);
            response.body = R"JSON({"code":200,"data":{"content":[{"id":"order-1","status":"reserved"}]}})JSON";
        } else if (request.url.find("https://cgyy.buaa.edu.cn/venue-zhjs-server/api/orders/lock/code?") == 0) {
            ++lock_code_requests;
            response.body = R"JSON({"code":200,"data":{"code":"123456"}})JSON";
        } else if (request.url == "https://app.buaa.edu.cn/uc/wap/notice/list") {
            CHECK(request.headers.at("Accept") == "application/json, text/javascript, */*; q=0.01");
            CHECK(request.headers.at("X-Requested-With") == "XMLHttpRequest");
            response.body = R"JSON({"code":0,"data":{"list":[{"id":"ann-1","title":"系统公告","status":"published","publishTime":"2026-05-18"},{"noticeId":2,"name":"数字公告","state":1}]}})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"code":404,"message":"unexpected request"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
    int lock_code_requests = 0;
};

class AnnouncementExpiredHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        (void)request;
        ++request_count;
        UBAANext::HttpResponse response;
        response.status_code = 302;
        response.headers["Location"] = "https://sso.buaa.edu.cn/login?service=app";
        response.body = "统一身份认证";
        return response;
    }

    int request_count = 0;
};

} // namespace

TEST_CASE("FeatureService 公告真实协议解析列表", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list("announcement", "list");

    REQUIRE(result);
    REQUIRE(result->size() == 2);
    CHECK((*result)[0].id == "ann-1");
    CHECK((*result)[0].title == "系统公告");
    CHECK((*result)[0].status == "published");
    CHECK((*result)[1].id == "2");
    CHECK((*result)[1].title == "数字公告");
    CHECK((*result)[1].status == "1");
}

TEST_CASE("FeatureService 公告真实协议识别 SSO 会话失效", "[service][feature][session]") {
    AnnouncementExpiredHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list("announcement", "list");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::SessionExpired);
    CHECK(http_client.request_count == 1);
}

TEST_CASE("FeatureService app version 真实协议本地返回", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list("app", "version");

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "app-version");
    CHECK(http_client.request_counts.empty());
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

TEST_CASE("FeatureService 泛化分页拒绝非数字参数", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto cgyy_result = service.list("cgyy", "orders:x:20");
    auto libbook_result = service.list("libbook", "reservations:1:x");

    REQUIRE_FALSE(cgyy_result);
    CHECK(cgyy_result.error().code == UBAANext::ErrorCode::InvalidArgument);
    REQUIRE_FALSE(libbook_result);
    CHECK(libbook_result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.request_counts.empty());
}

TEST_CASE("FeatureService 泛化 CGYY 订单保留零基页码", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list("cgyy", "orders:0:20");

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "order-1");
}

TEST_CASE("FeatureService 真实写兼容层不执行 typed service 写操作", "[service][feature]") {
    FeatureFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.mutate("bykc", "sign:x", "1", true);

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    CHECK(http_client.request_counts.empty());
}

TEST_CASE("FeatureService mock 写兼容层保留安全门", "[service][feature]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::FeatureService service(http_client, cache, UBAANext::ConnectionMode::Mock);

    auto blocked = service.mutate("signin", "do", "course-1", false);
    auto accepted = service.mutate("signin", "do", "course-1", true);

    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().code == UBAANext::ErrorCode::InvalidArgument);
    REQUIRE(accepted);
    CHECK(accepted->accepted);
    CHECK(accepted->summary.id == "course-1");
}
