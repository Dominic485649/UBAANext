#include <UBAANext/Service/BykcService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

namespace {

constexpr const char *kLoginUrl = "https://bykc.buaa.edu.cn/sscv/cas/login";
constexpr const char *kSsoRedirectUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fbykc.buaa.edu.cn%2Fsscv%2Fcas%2Flogin";
constexpr const char *kProfileUrl = "https://bykc.buaa.edu.cn/sscv/getUserProfile";

class BykcRedirectFixtureHttpClient : public UBAANext::IHttpClient {
public:
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
        } else {
            response.status_code = 404;
            response.body = R"JSON({"success":false,"errmsg":"unexpected url"})JSON";
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
