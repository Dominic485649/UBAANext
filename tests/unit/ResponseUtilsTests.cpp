#include <UBAANext/Service/ResponseUtils.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("ServiceResponse 识别会话过期响应", "[service][response]") {
    UBAANext::HttpResponse status_response;
    status_response.status_code = 401;
    REQUIRE(UBAANext::ServiceResponse::is_session_expired_response(status_response));

    UBAANext::HttpResponse html_response;
    html_response.status_code = 200;
    html_response.body = R"(<html><form><input name="execution"></form>统一身份认证</html>)";
    REQUIRE(UBAANext::ServiceResponse::is_session_expired_response(html_response));
}

TEST_CASE("ServiceResponse 解析 envelope data 变体", "[service][response]") {
    UBAANext::HttpResponse response;
    response.status_code = 200;
    response.body = R"({"code":200,"message":"ok","content":{"list":[{"id":"1"}]}})";

    auto parsed = UBAANext::ServiceResponse::parse_json_response(response, "测试");
    REQUIRE(parsed);
    REQUIRE(parsed->contains("list"));
    REQUIRE((*parsed)["list"].is_array());
}

TEST_CASE("ServiceResponse 业务未登录消息返回 SessionExpired", "[service][response]") {
    UBAANext::HttpResponse response;
    response.status_code = 200;
    response.body = R"({"code":500,"message":"登录失效，请重新登录"})";

    auto parsed = UBAANext::ServiceResponse::parse_json_response(response, "测试");
    REQUIRE_FALSE(parsed);
    REQUIRE(parsed.error().code == UBAANext::ErrorCode::SessionExpired);
}

TEST_CASE("ServiceResponse 非法 JSON 返回 ParseError", "[service][response]") {
    UBAANext::HttpResponse response;
    response.status_code = 200;
    response.body = "<html>not json</html>";

    auto parsed = UBAANext::ServiceResponse::parse_json_response(response, "测试");
    REQUIRE_FALSE(parsed);
    REQUIRE(parsed.error().code == UBAANext::ErrorCode::ParseError);
}
