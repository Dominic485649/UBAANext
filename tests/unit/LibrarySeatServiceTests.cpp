#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>

namespace {

class LibrarySeatFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        if (request.url == "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbooking.lib.buaa.edu.cn%2Fv4%2Flogin%2Fcas") {
            UBAANext::HttpResponse response;
            response.status_code = 302;
            response.headers["Location"] = "https://booking.lib.buaa.edu.cn/v4/login/cas?cas=test-cas";
            return response;
        }
        if (request.url == "https://booking.lib.buaa.edu.cn/v4/login/user") {
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = R"JSON({"code":0,"data":{"member":{"token":"test-token"}}})JSON";
            return response;
        }
        if (request.url == "https://booking.lib.buaa.edu.cn/v4/space/confirm") {
            ++confirm_requests;
            last_confirm_body = request.body;
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = confirm_body.empty() ? R"JSON({"code":0,"message":"预约成功","data":{"id":"booking-1"}})JSON" : confirm_body;
            return response;
        }
        if (request.url == "https://booking.lib.buaa.edu.cn/v4/space/cancel") {
            ++cancel_requests;
            last_cancel_body = request.body;
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = cancel_body.empty() ? R"JSON({"code":0,"message":"取消成功"})JSON" : cancel_body;
            return response;
        }
        UBAANext::HttpResponse response;
        response.status_code = 404;
        response.body = R"JSON({"code":404,"message":"unexpected url"})JSON";
        return response;
    }

    int confirm_requests = 0;
    int cancel_requests = 0;
    std::string confirm_body;
    std::string cancel_body;
    std::string last_confirm_body;
    std::string last_cancel_body;
};

} // namespace

TEST_CASE("LibrarySeatService 预约保留时段参数", "[service][libbook]") {
    LibrarySeatFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.reserve_seat("seat-1", "2026-05-15", "08:00-10:00");

#ifdef _WIN32
    REQUIRE(result);
    REQUIRE(http_client.confirm_requests == 1);
    CHECK(result->summary.fields.at("startTime") == "08:00");
    CHECK(result->summary.fields.at("endTime") == "10:00");
    CHECK(result->summary.fields.at("segment") == "08:00-10:00");
    auto body = nlohmann::json::parse(http_client.last_confirm_body);
    REQUIRE(body.contains("aesjson"));
    CHECK(body["aesjson"].is_string());
#else
    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NotImplemented);
    REQUIRE(http_client.confirm_requests == 0);
#endif
}

TEST_CASE("LibrarySeatService 预约接受显式起止时间", "[service][libbook]") {
    LibrarySeatFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.reserve_seat("seat-1", "2026-05-15", "", "10:00", "12:00");

#ifdef _WIN32
    REQUIRE(result);
    CHECK(result->summary.fields.at("startTime") == "10:00");
    CHECK(result->summary.fields.at("endTime") == "12:00");
#else
    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NotImplemented);
#endif
}

TEST_CASE("LibrarySeatService 预约识别业务失败消息", "[service][libbook]") {
    LibrarySeatFixtureHttpClient http_client;
    http_client.confirm_body = R"JSON({"code":0,"message":"该座位已被预约","data":{}})JSON";
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.reserve_seat("seat-1", "2026-05-15", "08:00-10:00");

#ifdef _WIN32
    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(result.error().message == "该座位已被预约");
#else
    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NotImplemented);
#endif
}

TEST_CASE("LibrarySeatService 取消识别业务失败消息", "[service][libbook]") {
    LibrarySeatFixtureHttpClient http_client;
    http_client.cancel_body = R"JSON({"code":0,"message":"该预约已取消"})JSON";
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.cancel_booking("booking-1");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(result.error().message == "该预约已取消");
}
