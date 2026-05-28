#include <UBAANext/Service/LibrarySeatService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace {

std::string direct_libbook_url(const std::string &url) {
    const auto prefix = std::string("https://d.buaa.edu.cn/https/");
    auto pos = url.find("/v4/", prefix.size());
    return pos == std::string::npos ? url : "https://booking.lib.buaa.edu.cn" + url.substr(pos);
}

std::string libbook_request_url(const UBAANext::HttpRequest &request) {
    if (request.url.find("/login") != std::string::npos && request.url.find("service=") != std::string::npos) {
        return request.url;
    }
    return direct_libbook_url(request.url);
}

class LibrarySeatFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        const auto url = libbook_request_url(request);
        if (request.url == "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbooking.lib.buaa.edu.cn%2Fv4%2Flogin%2Fcas" ||
            (request.url.find("/https/") != std::string::npos && request.url.find("/login") != std::string::npos)) {
            UBAANext::HttpResponse response;
            response.status_code = 302;
            response.headers["Location"] = "https://booking.lib.buaa.edu.cn/v4/login/cas?cas=test-cas";
            return response;
        }
        if (url == "https://booking.lib.buaa.edu.cn/v4/login/user") {
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = R"JSON({"code":0,"data":{"member":{"token":"test-token"}}})JSON";
            return response;
        }
        if (url == "https://booking.lib.buaa.edu.cn/v4/space/confirm") {
            ++confirm_requests;
            last_confirm_body = request.body;
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = confirm_body.empty() ? R"JSON({"code":0,"message":"预约成功","data":{"id":"booking-1"}})JSON" : confirm_body;
            return response;
        }
        if (url == "https://booking.lib.buaa.edu.cn/v4/space/cancel") {
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

class LibrarySeatRetryFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        const auto url = libbook_request_url(request);
        if (request.url.find("login") != std::string::npos && request.url.find("service=") != std::string::npos) {
            UBAANext::HttpResponse response;
            response.status_code = 302;
            response.headers["Location"] = "https://booking.lib.buaa.edu.cn/v4/login/cas?cas=cas-" + std::to_string(login_requests + 1);
            return response;
        }
        if (url == "https://booking.lib.buaa.edu.cn/v4/login/user") {
            ++login_requests;
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = std::string(R"JSON({"code":0,"data":{"member":{"token":"token-)JSON") + std::to_string(login_requests) + R"JSON("}}})JSON";
            return response;
        }
        if (url == "https://booking.lib.buaa.edu.cn/v4/member/seat" || url == "https://booking.lib.buaa.edu.cn/v4/user/reserveList") {
            ++seat_requests;
            seen_authorizations.push_back(request.headers.at("Authorization"));
            UBAANext::HttpResponse response;
            response.status_code = 200;
            response.body = seat_body.empty()
                                ? (seat_requests == 1
                                       ? R"JSON({"code":0,"message":"登录失效","data":{}})JSON"
                                       : R"JSON({"code":0,"data":{"list":[{"id":"booking-1","title":"座位预约","areaName":"A区","day":"2026-05-15","beginTime":"08:00","endTime":"10:00","statusName":"已预约"}]}})JSON")
                                : seat_body;
            return response;
        }
        UBAANext::HttpResponse response;
        response.status_code = 404;
        response.body = R"JSON({"code":404,"message":"unexpected url"})JSON";
        return response;
    }

    int login_requests = 0;
    int seat_requests = 0;
    std::string seat_body;
    std::vector<std::string> seen_authorizations;
};

} // namespace

TEST_CASE("LibrarySeatService 预约保留时段参数", "[service][libbook]") {
    LibrarySeatFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "libbook book"));

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
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "libbook book"));

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
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "libbook book"));

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

TEST_CASE("LibrarySeatService 查询业务错误消息会脱敏", "[service][libbook][security]") {
    LibrarySeatRetryFixtureHttpClient http_client;
    http_client.seat_body = R"JSON({"code":2,"message":"token=token-secret&Authorization: bearer-secret&photo_path=C:/secret/seat.txt","data":{}})JSON";
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.reservations();

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(result.error().message.find("token-secret") == std::string::npos);
    CHECK(result.error().message.find("bearer-secret") == std::string::npos);
    CHECK(result.error().message.find("C:/secret/seat.txt") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);
    CHECK(http_client.login_requests == 1);
    CHECK(http_client.seat_requests == 1);
}

TEST_CASE("LibrarySeatService 授权请求过期后刷新 token 并重试一次", "[service][libbook]") {
    LibrarySeatRetryFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::LibrarySeatService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.reservations();

    REQUIRE(result);
    CHECK(http_client.login_requests == 2);
    CHECK(http_client.seat_requests == 2);
    REQUIRE(http_client.seen_authorizations.size() == 2);
    CHECK(http_client.seen_authorizations[0] == "bearertoken-1");
    CHECK(http_client.seen_authorizations[1] == "bearertoken-2");
}
