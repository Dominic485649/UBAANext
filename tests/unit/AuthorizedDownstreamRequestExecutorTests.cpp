#include <UBAANext/Protocol/AuthorizedDownstreamRequestExecutor.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace {

class AuthorizedExecutorFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++requests;
        seen_authorizations.push_back(request.headers.at("Authorization"));
        UBAANext::HttpResponse response;
        response.status_code = 200;
        response.body = requests == 1 ? "expired" : "ok";
        return response;
    }

    int requests = 0;
    std::vector<std::string> seen_authorizations;
};

class AlwaysExpiredFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++requests;
        seen_authorizations.push_back(request.headers.at("Authorization"));
        UBAANext::HttpResponse response;
        response.status_code = 200;
        response.body = "expired";
        return response;
    }

    int requests = 0;
    std::vector<std::string> seen_authorizations;
};

} // namespace

TEST_CASE("AuthorizedDownstreamRequestExecutor refreshes and retries once", "[protocol][authorized]" ) {
    AuthorizedExecutorFixtureHttpClient http_client;
    int ensure_calls = 0;
    int invalidations = 0;
    std::string token = "token-1";

    UBAANext::HttpRequest request;
    request.method = UBAANext::HttpMethod::Get;
    request.url = "https://example.edu/api";

    UBAANext::Protocol::AuthorizedRequestHooks hooks;
    hooks.system = UBAANext::Protocol::DownstreamSystemId::LibBook;
    hooks.expired_message = "测试会话已过期";
    hooks.ensure_authorized = [&](bool force_refresh) -> UBAANext::Result<void> {
        ++ensure_calls;
        if (force_refresh) token = "token-2";
        return {};
    };
    hooks.invalidate = [&]() { ++invalidations; token.clear(); };
    hooks.decorate_request = [&](UBAANext::HttpRequest &authorized_request) {
        authorized_request.headers["Authorization"] = "Bearer " + token;
    };
    hooks.is_expired_response = [](const UBAANext::HttpResponse &response) {
        return response.body == "expired";
    };

    auto response = UBAANext::Protocol::send_authorized_request(http_client, request, hooks);

    REQUIRE(response);
    CHECK(response->body == "ok");
    CHECK(ensure_calls == 2);
    CHECK(invalidations == 1);
    CHECK(http_client.requests == 2);
    REQUIRE(http_client.seen_authorizations.size() == 2);
    CHECK(http_client.seen_authorizations[0] == "Bearer token-1");
    CHECK(http_client.seen_authorizations[1] == "Bearer token-2");
}

TEST_CASE("AuthorizedDownstreamRequestExecutor does not retry when disabled", "[protocol][authorized]" ) {
    AlwaysExpiredFixtureHttpClient http_client;
    int ensure_calls = 0;
    int invalidations = 0;
    std::string token = "token-1";

    UBAANext::HttpRequest request;
    request.method = UBAANext::HttpMethod::Get;
    request.url = "https://example.edu/api";

    UBAANext::Protocol::AuthorizedRequestHooks hooks;
    hooks.system = UBAANext::Protocol::DownstreamSystemId::LibBook;
    hooks.expired_message = "测试会话已过期";
    hooks.ensure_authorized = [&](bool force_refresh) -> UBAANext::Result<void> {
        ++ensure_calls;
        if (force_refresh) token = "token-2";
        return {};
    };
    hooks.invalidate = [&]() { ++invalidations; token.clear(); };
    hooks.decorate_request = [&](UBAANext::HttpRequest &authorized_request) {
        authorized_request.headers["Authorization"] = "Bearer " + token;
    };
    hooks.is_expired_response = [](const UBAANext::HttpResponse &response) {
        return response.body == "expired";
    };

    auto response = UBAANext::Protocol::send_authorized_request(http_client, request, hooks, true, false);

    REQUIRE_FALSE(response);
    CHECK(ensure_calls == 1);
    CHECK(invalidations == 1);
    CHECK(http_client.requests == 1);
}
