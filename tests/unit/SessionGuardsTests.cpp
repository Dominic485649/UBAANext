#include <UBAANext/Protocol/SessionGuards.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SessionGuards detects SSO login pages and URLs", "[protocol][session]") {
    UBAANext::HttpResponse response;
    response.status_code = 200;
    response.body = R"HTML(<!DOCTYPE html><html><form><input name="execution" value="e1"/><input name="type" value="username_password"/></form></html>)HTML";

    CHECK(UBAANext::Protocol::is_session_expired_response(response, "https://bykc.buaa.edu.cn/sscv/getUserProfile", true));
    CHECK(UBAANext::Protocol::is_session_expired_response(response, "https://sso.buaa.edu.cn/login?service=x", false));
}

TEST_CASE("SessionGuards does not classify non-login business HTML as expired", "[protocol][session]") {
    UBAANext::HttpResponse response;
    response.status_code = 200;
    response.body = R"HTML(<html><body><h1>作业列表</h1><table><tr><td>assignment</td></tr></table></body></html>)HTML";

    CHECK_FALSE(UBAANext::Protocol::is_session_expired_response(response, "https://judge.buaa.edu.cn/assignment/index.jsp", false));
    CHECK_FALSE(UBAANext::Protocol::is_session_expired_response(response, "https://judge.buaa.edu.cn/assignment/index.jsp", true));
}

TEST_CASE("SessionGuards detects redirect Location to SSO", "[protocol][session]") {
    UBAANext::HttpResponse response;
    response.status_code = 302;
    response.headers["Location"] = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fexample.edu";

    CHECK(UBAANext::Protocol::is_session_expired_response(response));
}
