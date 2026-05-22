#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr const char *kSpocLoginResponse = R"JSON({"code":200,"content":{"jsdm":"student"}})JSON";
constexpr const char *kSpocTermResponse = R"JSON({"code":200,"content":{"mrxq":"2025-2026-2","dqxq":"2025-2026学年第二学期"}})JSON";
constexpr const char *kSpocCoursesResponse = R"JSON({"code":200,"content":[{"kcid":"course-1","kcmc":"智能制造导论","skjs":"张老师"}]})JSON";
constexpr const char *kSpocAssignmentsResponse = R"JSON({"code":200,"content":{"list":[{"zyid":"spoc-3","sskcid":"course-1","zymc":"已交作业","tjzt":"已提交","zyjzsj":"2026-03-20 23:59:00"},{"zyid":"spoc-2","sskcid":"course-1","zymc":"过期作业","tjzt":"已过期","zyjzsj":"2026-03-08 23:59:00"},{"zyid":"spoc-1","sskcid":"course-1","zymc":"未交作业","tjzt":"未做","zyjzsj":"2026-03-10 23:59:00"}],"hasNextPage":false,"pages":1}})JSON";

std::string direct_spoc_url(const std::string &url) {
    const auto prefix = std::string("https://d.buaa.edu.cn/https/");
    auto pos = url.find("/spocnew", prefix.size());
    return pos == std::string::npos ? url : "https://spoc.buaa.edu.cn" + url.substr(pos);
}

class SpocFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        const auto url = direct_spoc_url(request.url);
        UBAANext::HttpResponse response;
        if (url == "https://spoc.buaa.edu.cn/spocnewht/cas") {
            response.status_code = 302;
            response.headers["Location"] = "https://spoc.buaa.edu.cn/spocnew/cas?token=test-token";
            return response;
        }
        response.status_code = 200;
        if (url == "https://spoc.buaa.edu.cn/spocnewht/sys/casLogin") response.body = kSpocLoginResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne") response.body = kSpocTermResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=2025-2026-2") response.body = kSpocCoursesResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage") response.body = kSpocAssignmentsResponse;
        else response.body = R"JSON({"code":404,"msg":"unexpected url"})JSON";
        return response;
    }
};

class SpocTlsFallbackHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        const auto url = direct_spoc_url(request.url);
        if (url == "https://spoc.buaa.edu.cn/spocnewht/cas") {
            ++direct_cas_requests;
            return UBAANext::make_error(UBAANext::ErrorCode::TlsError, "Curl TLS 失败: schannel handshake failed");
        }
        UBAANext::HttpResponse response;
        if (request.url.find("sso.buaa.edu.cn/login") != std::string::npos || request.url.find("/https/") != std::string::npos && request.url.find("/login") != std::string::npos) {
            ++sso_requests;
            response.status_code = 302;
            response.headers["Location"] = "https://spoc.buaa.edu.cn/spocnew/cas?token=test-token";
            return response;
        }
        response.status_code = 200;
        const auto business_url = direct_spoc_url(request.url);
        if (business_url == "https://spoc.buaa.edu.cn/spocnewht/sys/casLogin") response.body = kSpocLoginResponse;
        else if (business_url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne") response.body = kSpocTermResponse;
        else if (business_url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=2025-2026-2") response.body = kSpocCoursesResponse;
        else if (business_url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage") response.body = kSpocAssignmentsResponse;
        else response.body = R"JSON({"code":404,"msg":"unexpected url"})JSON";
        return response;
    }

    int direct_cas_requests = 0;
    int sso_requests = 0;
};

class SpocVpnFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        if (request.url.rfind("https://d.buaa.edu.cn/", 0) != 0) {
            direct_requests++;
        }
        return fixture.send(request);
    }

    SpocFixtureHttpClient fixture;
    int direct_requests = 0;
};

UBAANext::SpocService make_spoc_service(SpocFixtureHttpClient &http_client, UBAANext::MemoryCacheStore &cache) {
    return UBAANext::SpocService(http_client, cache, UBAANext::ConnectionMode::Direct);
}

} // namespace

TEST_CASE("SpocService 按状态过滤待办和过期作业", "[service][spoc]") {
    SpocFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    UBAANext::SpocAssignmentQuery query;
    auto active = service.list_assignment_summaries(query);
    REQUIRE(active);
    REQUIRE(active->size() == 2);
    CHECK((*active)[0].id == "spoc-1");
    CHECK((*active)[0].status == "unsubmitted");
    CHECK((*active)[1].id == "spoc-3");
    CHECK((*active)[1].status == "submitted");

    query.pending_only = true;
    auto pending = service.list_assignment_summaries(query);
    REQUIRE(pending);
    REQUIRE(pending->size() == 1);
    CHECK((*pending)[0].id == "spoc-1");

    query.include_expired = true;
    auto pending_with_expired = service.list_assignment_summaries(query);
    REQUIRE(pending_with_expired);
    REQUIRE(pending_with_expired->size() == 2);
    CHECK((*pending_with_expired)[0].id == "spoc-2");
    CHECK((*pending_with_expired)[0].status == "expired");
    CHECK((*pending_with_expired)[1].id == "spoc-1");
}

TEST_CASE("SpocService VPN 模式下所有 SPOC 请求走 WebVPN", "[service][spoc]") {
    SpocVpnFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::SpocService service(http_client, cache, UBAANext::ConnectionMode::WebVPN);

    auto result = service.list_assignment_summaries();

    REQUIRE(result);
    CHECK(http_client.direct_requests == 0);
}
TEST_CASE("SpocService 直连 CAS TLS 失败时回退到 SSO service 链", "[service][spoc]") {
    SpocTlsFallbackHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::SpocService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    UBAANext::SpocAssignmentQuery query;
    auto result = service.list_assignment_summaries(query);

    REQUIRE(result);
    CHECK(http_client.direct_cas_requests == 1);
    CHECK(http_client.sso_requests == 1);
    CHECK_FALSE(result->empty());
}
