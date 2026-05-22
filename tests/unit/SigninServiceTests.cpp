#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

std::string today_yyyymmdd() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d");
    return out.str();
}

} // namespace

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

TEST_CASE("SigninService 今日列表识别业务失败后降级为空列表", "[service][signin]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;

    http_client.set_mock_response("https://uc.buaa.edu.cn/api/uc/userinfo", R"({"code":0,"data":{"schoolid":"20260000"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=", R"({"STATUS":0,"result":{"id":"user-1","sessionId":"session-1"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/course/get_stu_course_sched.action?id=user-1&dateStr=" + today_yyyymmdd(), R"({"STATUS":"401","ERRMSG":"登录失效"})");

    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    auto result = service.list_today_courses();

    REQUIRE(result);
    CHECK(result->empty());
}

TEST_CASE("SigninService 今日列表重试后仍失败时返回空列表", "[service][signin]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;

    http_client.set_mock_response("https://uc.buaa.edu.cn/api/uc/userinfo", R"({"code":0,"data":{"schoolid":"20260000"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=", R"({"STATUS":0,"result":{"id":"user-1","sessionId":"session-1"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/course/get_stu_course_sched.action?id=user-1&dateStr=" + today_yyyymmdd(), R"({"STATUS":"401","ERRMSG":"登录失效"})");

    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    auto result = service.list_today_courses();

    REQUIRE(result);
    CHECK(result->empty());
}

TEST_CASE("SigninService 提交识别业务失败", "[service][signin]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;

    http_client.set_mock_response("https://uc.buaa.edu.cn/api/uc/userinfo", R"({"code":0,"data":{"schoolid":"20260000"}})");
    http_client.set_mock_response("https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=", R"({"STATUS":"200","result":{"id":"user-1","sessionId":"session-1"}})");
    http_client.set_mock_response("http://iclass.buaa.edu.cn:8081/app/common/get_timestamp.action", R"({"timestamp":"123456"})");

    const std::string submit_url = "http://iclass.buaa.edu.cn:8081/app/course/stu_scan_sign.action?courseSchedId=course-1&timestamp=123456";
    http_client.set_mock_response(submit_url, R"({"STATUS":"400","result":{"stuSignStatus":"0"},"ERRMSG":"签到未开始"})");

    UBAANext::SigninService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    auto result = service.perform_signin("course-1");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(result.error().message == "当前还未到签到时间");
}
