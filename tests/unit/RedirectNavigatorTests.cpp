#include <UBAANext/Protocol/RedirectNavigator.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("RedirectNavigator resolves relative and protocol-relative locations", "[protocol][redirect]") {
    using UBAANext::Protocol::resolve_location;

    CHECK(resolve_location("https://example.edu/a/b/index.html?x=1", "next") == "https://example.edu/a/b/next");
    CHECK(resolve_location("https://example.edu/a/b/index.html?x=1", "/root") == "https://example.edu/root");
    CHECK(resolve_location("https://example.edu/a/b/index.html?x=1", "?y=2") == "https://example.edu/a/b/index.html?y=2");
    CHECK(resolve_location("https://example.edu/a/b/index.html?x=1", "//cdn.example.edu/app") == "https://cdn.example.edu/app");
    CHECK(resolve_location("https://example.edu/a/b/index.html?x=1", "https://other.example.edu/ok") == "https://other.example.edu/ok");
}

TEST_CASE("RedirectNavigator extracts query parameters and redacts sensitive query", "[protocol][redirect]") {
    using namespace UBAANext::Protocol;

    CHECK(extract_query_parameter("https://example.edu/callback?ticket=ST-1&cas=CAS-1#frag", "ticket") == "ST-1");
    CHECK(extract_query_parameter("https://example.edu/callback?ticket=ST-1&cas=CAS-1#frag", "cas") == "CAS-1");
    CHECK(extract_query_parameter("https://example.edu/callback?ticket=ST-1&cas=CAS-1#frag", "missing").empty());
    CHECK(extract_query_parameter_anywhere("https://d.buaa.edu.cn/https/enc/path?redirect=https%3A%2F%2Fbooking.lib.buaa.edu.cn%2Fv4%2Flogin%2Fcas%3Fcas%3DCAS-2", "cas") == "CAS-2");
    CHECK(extract_query_parameter_anywhere("https://ygdk.buaa.edu.cn/#/home?code=OAUTH-1", "code") == "OAUTH-1");

    std::string large_body(200000, 'x');
    large_body += "?redirect=https%3A%2F%2Fbooking.lib.buaa.edu.cn%2Fv4%2Flogin%2Fcas%3Fcas%3DCAS-LARGE%26next%3Dok";
    large_body += std::string(200000, 'y');
    CHECK(extract_query_parameter_anywhere(large_body, "cas") == "CAS-LARGE");

    CHECK(redact_url_query("https://example.edu/callback?ticket=ST-1&cas=CAS-1#frag") == "https://example.edu/callback?<redacted>#frag");
    CHECK(redact_url_query("https://example.edu/callback") == "https://example.edu/callback");
}
