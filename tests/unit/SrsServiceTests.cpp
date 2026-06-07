#include <UBAANext/Service/SrsService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace um = UBAANext;

namespace {

class SrsFixtureHttpClient : public um::IHttpClient {
public:
    um::Result<um::HttpResponse> send(const um::HttpRequest &request) override {
        requests.push_back(request);
        if (next < responses.size()) return responses[next++];
        um::HttpResponse response;
        response.status_code = 200;
        response.body = R"JSON({"code":200,"msg":"ok","data":null})JSON";
        return response;
    }

    void push(int status, std::string body, std::unordered_map<std::string, std::string> headers = {}) {
        um::HttpResponse response;
        response.status_code = status;
        response.body = std::move(body);
        response.headers = std::move(headers);
        responses.push_back(std::move(response));
    }

    std::vector<um::HttpRequest> requests;
    std::vector<um::HttpResponse> responses;
    std::size_t next = 0;
};

class SrsFixtureCookieStore : public um::ICookieStore {
public:
    um::Result<um::CookieJar> load() override { return jar; }
    um::Result<void> save(const um::CookieJar &cookies) override {
        jar = cookies;
        return {};
    }
    um::Result<void> save_current() override { return {}; }
    um::Result<void> clear() override {
        jar.clear();
        return {};
    }
    const um::CookieJar *current() const override { return &jar; }

    um::CookieJar jar;
};

um::WriteOperationGate enabled_gate(std::string operation) {
    um::WriteOperationGate gate;
    gate.confirmed = true;
    gate.allow_write_operations = true;
    gate.operation = std::move(operation);
    return gate;
}

} // namespace

TEST_CASE("SrsService 查询课程发送 buaa-api 对齐字段", "[service][srs]") {
    SrsFixtureHttpClient http;
    http.push(200, R"JSON({"code":200,"msg":"ok","data":{"rows":[{"JXBID":"clazz-1","KCM":"编译原理","KCH":"B3","KXH":"01","XQ":"1","SFCT":"0","SFYX":"0","SKJSZC":"张老师","secretVal":"secret-1"}]}})JSON");
    SrsFixtureCookieStore cookies;
    cookies.jar.set_cookie("byxk.buaa.edu.cn", "token", "srs-token");
    um::MemoryCacheStore cache;
    um::SrsService service(http, &cookies, cache, um::ConnectionMode::Direct);

    um::Model::SrsCourseFilter filter;
    filter.scope = "ALLKC";
    filter.page = 2;
    filter.size = 30;
    filter.campus = 2;
    filter.keyword = "编译";
    auto result = service.courses(filter);

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "clazz-1");
    CHECK((*result)[0].fields.at("secretVal") == "secret-1");
    REQUIRE(http.requests.size() == 1);
    CHECK(http.requests[0].headers.at("Authorization") == "srs-token");
    auto body = nlohmann::json::parse(http.requests[0].body);
    CHECK(body["teachingClassType"] == "ALLKC");
    CHECK(body["pageNumber"] == 2);
    CHECK(body["pageSize"] == 30);
    CHECK(body["campus"] == 2);
    CHECK(body["KEY"] == "编译");
}

TEST_CASE("SrsService 写操作未确认不发请求", "[service][srs][write]") {
    SrsFixtureHttpClient http;
    SrsFixtureCookieStore cookies;
    cookies.jar.set_cookie("byxk.buaa.edu.cn", "token", "srs-token");
    um::MemoryCacheStore cache;
    um::SrsService service(http, &cookies, cache, um::ConnectionMode::Direct);

    um::Model::SrsCourseOperation operation;
    operation.scope = "ALLKC";
    operation.class_id = "clazz-1";
    operation.secret = "secret-1";
    auto result = service.select_course(operation);

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
    CHECK(http.requests.empty());
}

TEST_CASE("SrsService 正选课程使用 form payload", "[service][srs][write]") {
    SrsFixtureHttpClient http;
    http.push(200, R"JSON({"code":200,"msg":"ok","data":null})JSON");
    SrsFixtureCookieStore cookies;
    cookies.jar.set_cookie("byxk.buaa.edu.cn", "token", "srs-token");
    um::MemoryCacheStore cache;
    um::SrsService service(http, &cookies, cache, um::ConnectionMode::Direct);
    service.set_write_operation_gate(enabled_gate("srs course select"));

    um::Model::SrsCourseOperation operation;
    operation.scope = "ALLKC";
    operation.class_id = "clazz-1";
    operation.secret = "secret-1";
    auto result = service.select_course(operation);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 1);
    CHECK(http.requests[0].headers.at("Content-Type") == "application/x-www-form-urlencoded");
    CHECK(http.requests[0].body.find("clazzType=ALLKC") != std::string::npos);
    CHECK(http.requests[0].body.find("clazzId=clazz-1") != std::string::npos);
    CHECK(http.requests[0].body.find("secretVal=secret-1") != std::string::npos);
}
