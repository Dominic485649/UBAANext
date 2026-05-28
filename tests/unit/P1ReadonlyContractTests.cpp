#include <UBAANext/Protocol/AppBuaaSession.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/TermService.hpp>

#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

namespace {

class ByxtExpiredHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url.find("currentUser.do") != std::string::npos) {
            response.body = R"({"url":"/login"})";
        } else if (request.url.find("getUserInfo.do") != std::string::npos) {
            response.body = R"({"code":"1","data":null})";
        } else {
            response.body = R"(<html><input name="execution" value="secret-token"></html>)";
        }
        return response;
    }
};

class ByxtRedirectCapturingHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        if (request.url.find("currentUser.do") != std::string::npos) {
            ++probe_requests;
        }
        if (request.url.find("getUserInfo.do") != std::string::npos) {
            ++graduate_probe_requests;
        }
        if (request.url.find("sso.buaa.edu.cn/login") != std::string::npos) {
            app_follow_redirects = request.redirect.follow_redirects;
        }

        UBAANext::HttpResponse response;
        response.status_code = 200;
        response.body = R"(<html><input name="execution" value="secret-token"></html>)";
        return response;
    }

    int probe_requests = 0;
    int graduate_probe_requests = 0;
    bool app_follow_redirects = true;
};

class ByxtBusinessHtmlHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        if (request.url.find("currentUser.do") != std::string::npos) {
            ++undergrad_probes;
        } else if (request.url.find("getUserInfo.do") != std::string::npos) {
            ++graduate_probes;
        } else if (request.url.find("sso.buaa.edu.cn/login") != std::string::npos) {
            ++activation_requests;
        }

        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url.find("currentUser.do") != std::string::npos) {
            response.body = R"(<html><body>BYXT warmup page</body></html>)";
        } else if (request.url.find("getUserInfo.do") != std::string::npos) {
            response.body = R"({"code":"1","data":null})";
        } else {
            response.body = R"(<html><body>BYXT application initialized</body></html>)";
        }
        return response;
    }

    int undergrad_probes = 0;
    int graduate_probes = 0;
    int activation_requests = 0;
};

class ByxtGraduateReadyHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url.find("currentUser.do") != std::string::npos) {
            response.body = R"(<html><body>undergrad portal unavailable</body></html>)";
        } else if (request.url.find("getUserInfo.do") != std::string::npos) {
            response.body = R"({"code":"0","data":{"userId":"student"}})";
        } else {
            response.body = R"(<html><body>unused</body></html>)";
        }
        return response;
    }
};

class GradeNetworkErrorHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::NetworkError, "token=token-secret&Cookie: SID=cookie-secret&photo_path=C:/secret/grade.html");
    }
};

} // namespace

TEST_CASE("P1 真实周课表拒绝隐式默认学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_week_courses(8);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 成绩系统网络错误会脱敏", "[p1][real-readonly][redaction]") {
    GradeNetworkErrorHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::GradeService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.list_grades("2025-2026-2");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::NetworkError);
    CHECK(result.error().message.find("token-secret") == std::string::npos);
    CHECK(result.error().message.find("cookie-secret") == std::string::npos);
    CHECK(result.error().message.find("C:/secret/grade.html") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);
}

TEST_CASE("P1 真实周课表拒绝无效周次", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::CourseService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_week_courses(0, "2025-2026-2");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实考试查询拒绝隐式默认学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::ExamService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_exams();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实周次查询拒绝空学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::TermService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.get_weeks("");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 真实成绩查询拒绝空学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::GradeService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.list_grades("");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("P1 Score session 识别登录失效响应", "[p1][real-readonly]") {
    um::HttpResponse response;
    response.status_code = 200;
    response.body = R"({"message":"请重新登录"})";

    CHECK(um::Protocol::Score::is_session_expired_response(response));
}

TEST_CASE("P1 session guard 大小写无关识别 SSO Location", "[p1][session]") {
    um::HttpResponse response;
    response.status_code = 302;
    response.headers["location"] = "https://sso.buaa.edu.cn/login?service=example";

    CHECK(um::Protocol::is_session_expired_response(response));
    CHECK(um::Protocol::Byxt::is_session_expired_response(response));
    CHECK(um::Protocol::AppBuaa::is_session_expired_response(response));
}

TEST_CASE("P1 BYXT 会话激活错误不回显响应正文", "[p1][session][redaction]") {
    ByxtExpiredHttpClient http_client;

    auto result = um::Protocol::Byxt::ensure_session(http_client, um::ConnectionMode::Direct);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::SessionExpired);
    CHECK(result.error().message.find("secret-token") == std::string::npos);
    CHECK(result.error().message.find("execution") == std::string::npos);
}

TEST_CASE("P1 BYXT 激活入口禁用传输层自动重定向", "[p1][session][redirect]") {
    ByxtRedirectCapturingHttpClient http_client;

    auto result = um::Protocol::Byxt::ensure_session(http_client, um::ConnectionMode::Direct);

    REQUIRE_FALSE(result.has_value());
    CHECK_FALSE(http_client.app_follow_redirects);
    CHECK(http_client.probe_requests == 2);
    CHECK(http_client.graduate_probe_requests == 1);
}

TEST_CASE("P1 BYXT 200 非登录业务 HTML 不误报为需要 SSO", "[p1][session][byxt]") {
    ByxtBusinessHtmlHttpClient http_client;

    auto result = um::Protocol::Byxt::ensure_session(http_client, um::ConnectionMode::Direct);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::ParseError);
    CHECK(result.error().message.find("需要 SSO 认证") == std::string::npos);
    CHECK(result.error().message.find("execution") == std::string::npos);
    CHECK(http_client.activation_requests == 0);
}

TEST_CASE("P1 BYXT 研究生门户探测成功视为学籍门户就绪", "[p1][session][byxt]") {
    ByxtGraduateReadyHttpClient http_client;

    auto result = um::Protocol::Byxt::ensure_session(http_client, um::ConnectionMode::Direct);

    REQUIRE(result.has_value());
}

TEST_CASE("P1 真实成绩全部查询拒绝隐式学期", "[p1][real-readonly]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache_store;
    um::GradeService service(http_client, cache_store, um::ConnectionMode::Direct);

    auto result = service.list_all_grades();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
}
