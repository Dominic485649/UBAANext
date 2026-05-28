#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <utility>

namespace {

constexpr const char *kLoginUrl = "https://bykc.buaa.edu.cn/sscv/cas/login";
constexpr const char *kSsoRedirectUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbykc.buaa.edu.cn%2Fsscv%2Fcas%2Flogin";
constexpr const char *kProfileUrl = "https://bykc.buaa.edu.cn/sscv/getUserProfile";
constexpr const char *kCourseDetailUrl = "https://bykc.buaa.edu.cn/sscv/queryCourseById";
constexpr const char *kSignUrl = "https://bykc.buaa.edu.cn/sscv/signCourseByUser";

class BykcRedirectFixtureHttpClient : public UBAANext::IHttpClient {
public:
    explicit BykcRedirectFixtureHttpClient(std::string sign_config = {}) : sign_config(std::move(sign_config)) {}

    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == kLoginUrl) {
            response.status_code = 302;
            response.headers["Location"] = kSsoRedirectUrl;
        } else if (request.url == kSsoRedirectUrl) {
            response.status_code = 302;
            response.headers["Location"] = "/sscv/cas/login?token=token-1";
        } else if (request.url == kProfileUrl) {
            auto ak = request.headers.find("ak");
            auto sk = request.headers.find("sk");
            REQUIRE(ak != request.headers.end());
            REQUIRE(sk != request.headers.end());
            CHECK(request.headers.at("auth_token") == "token-1");
            CHECK(request.headers.at("authtoken") == "token-1");
            response.body = R"JSON({"status":"0","data":{"id":"user-1","realName":"测试用户","employeeId":"20260001","studentNo":"20260001","studentType":"本科生","classCode":"A101","college":{"collegeName":"学院"},"term":{"termName":"2025-2026"}}})JSON";
        } else if (request.url == kCourseDetailUrl) {
            response.body = std::string(R"JSON({"status":"0","data":{"id":"1","courseName":"测试课程","selected":true,"courseSignConfig":)JSON") + sign_config + "}}";
        } else if (request.url == kSignUrl) {
            response.body = R"JSON({"status":"0","data":{"accepted":true}})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"success":false,"errmsg":"unexpected url"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
    std::string sign_config = R"JSON("gps")JSON";
};

class BykcBusinessErrorFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == kLoginUrl) {
            response.status_code = 302;
            response.headers["Location"] = "/sscv/cas/login?token=token-1";
        } else if (request.url == kProfileUrl) {
            response.body = R"JSON({"status":"1","errmsg":"token=token-secret&Cookie: SID=cookie-secret&photo_path=C:/secret/photo.jpg"})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"status":"1","errmsg":"unexpected request"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
};

} // namespace

TEST_CASE("BykcService 跟随 CAS 多跳重定向获取 token", "[service][bykc]") {
    BykcRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::BykcService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.profile();

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "user-1");
    CHECK((*result)[0].title == "测试用户");
    CHECK(http_client.request_counts[kSsoRedirectUrl] == 1);
    CHECK(http_client.request_counts[kProfileUrl] == 1);
}

TEST_CASE("BykcService 签到从课程签到范围生成位置", "[service][bykc]") {
    BykcRedirectFixtureHttpClient http_client(R"JSON({"signPointList":[{"lat":40.1001,"lng":116.3001,"radius":8.0}]})JSON");
    UBAANext::MemoryCacheStore cache;
    UBAANext::BykcService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "bykc sign"));

    auto result = service.sign_course("1", 1);

    REQUIRE(result);
    CHECK(result->accepted);
    CHECK(result->message == "签到成功");
    CHECK(http_client.request_counts[kCourseDetailUrl] == 1);
    CHECK(http_client.request_counts[kSignUrl] == 1);
}

TEST_CASE("BykcService 无签到范围时不提交签到", "[service][bykc]") {
    BykcRedirectFixtureHttpClient http_client(R"JSON({"signPointList":[]})JSON");
    UBAANext::MemoryCacheStore cache;
    UBAANext::BykcService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "bykc sign"));

    auto result = service.sign_course("1", 1);

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.request_counts[kCourseDetailUrl] == 1);
    CHECK(http_client.request_counts[kSignUrl] == 0);
}

TEST_CASE("BykcService 拒绝非数字业务 ID", "[service][bykc]") {
    BykcRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::BykcService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "bykc write"));

    auto detail = service.show_course("course-x");
    auto select = service.select_course("course-x");
    auto unselect = service.unselect_course("chosen-x");
    auto sign = service.sign_course("course-x", 1);

    REQUIRE_FALSE(detail);
    CHECK(detail.error().code == UBAANext::ErrorCode::InvalidArgument);
    REQUIRE_FALSE(select);
    CHECK(select.error().code == UBAANext::ErrorCode::InvalidArgument);
    REQUIRE_FALSE(unselect);
    CHECK(unselect.error().code == UBAANext::ErrorCode::InvalidArgument);
    REQUIRE_FALSE(sign);
    CHECK(sign.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.request_counts.empty());
}

TEST_CASE("BykcService 业务错误消息会脱敏", "[service][bykc][security]") {
    BykcBusinessErrorFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::BykcService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.profile();

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(result.error().message.find("token-secret") == std::string::npos);
    CHECK(result.error().message.find("cookie-secret") == std::string::npos);
    CHECK(result.error().message.find("C:/secret/photo.jpg") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);
    CHECK(http_client.request_counts[kProfileUrl] == 1);
}
