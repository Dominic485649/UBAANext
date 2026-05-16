#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SigninService 写请求失败不重试", "[service][signin]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;

    http_client.set_mock_response("https://uc.buaa.edu.cn/api/uc/userinfo", R"({"code":0,"data":{"schoolid":"20260000"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=", R"({"STATUS":0,"result":{"id":"user-1","sessionId":"session-1"}})");
    http_client.set_mock_response("http://iclass.buaa.edu.cn:8081/app/common/get_timestamp.action", R"({"timestamp":"123456"})");

    const std::string submit_url = "http://iclass.buaa.edu.cn:8081/app/course/stu_scan_sign.action?courseSchedId=course-1&timestamp=123456";
    http_client.set_http_error(submit_url, 500, "{}");

    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    auto result = service.perform_signin("course-1");

    REQUIRE_FALSE(result);
    REQUIRE(http_client.request_count(submit_url) == 1);
}

TEST_CASE("SigninService 接受字符串形式状态码", "[service][signin]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;

    http_client.set_mock_response("https://uc.buaa.edu.cn/api/uc/userinfo", R"({"code":0,"data":{"schoolid":"20260000"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=", R"({"STATUS":"0","result":{"id":"user-1","sessionId":"session-1"}})");
    http_client.set_mock_response("http://iclass.buaa.edu.cn:8081/app/common/get_timestamp.action", R"({"timestamp":"123456"})");

    const std::string submit_url = "http://iclass.buaa.edu.cn:8081/app/course/stu_scan_sign.action?courseSchedId=course-1&timestamp=123456";
    http_client.set_mock_response(submit_url, R"({"STATUS":"0","result":{"stuSignStatus":"1"},"ERRMSG":"签到成功"})");

    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    auto result = service.perform_signin("course-1");

    REQUIRE(result);
    CHECK(result->accepted);
    CHECK(http_client.request_count(submit_url) == 1);
}
