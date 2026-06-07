#include <UBAANext/Parser/LiveParser.hpp>
#include <UBAANext/Service/LiveService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace um = UBAANext;

namespace {

class LiveFixtureHttpClient : public um::IHttpClient {
public:
    um::Result<um::HttpResponse> send(const um::HttpRequest &request) override {
        ++request_count;
        last_request = request;
        um::HttpResponse response;
        response.status_code = status_code;
        response.headers = headers;
        response.body = body;
        return response;
    }

    int request_count = 0;
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body = R"JSON({"success":true,"result":{"code":200,"msg":"","list":[{"course":[{"course_id":"course-1","id":"live-1","course_title":"计算机网络","teacher_name":"李老师"}]},{"course":[]},{"course":[]},{"course":[]},{"course":[]},{"course":[]},{"course":[]}]}})JSON";
    um::HttpRequest last_request;
};

} // namespace

TEST_CASE("parse_live_week_schedule_days 解析 reference 周课表 envelope list", "[LiveParser]") {
    const auto list = nlohmann::json::array({
        nlohmann::json{{"course", nlohmann::json::array({nlohmann::json{{"course_id", "course-1"}, {"id", "live-1"}, {"course_title", "计算机网络"}, {"teacher_name", "李老师"}}})}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array({nlohmann::json{{"course_id", 42}, {"id", 7}, {"course_title", "周日课程"}, {"teacher_name", "王老师"}, {"status", true}}})}},
    });

    auto days = um::Parser::parse_live_week_schedule_days(list);

    REQUIRE(days.size() == 7);
    REQUIRE(days[0].size() == 1);
    CHECK(days[0][0].course_id == "course-1");
    CHECK(days[0][0].live_id == "live-1");
    CHECK(days[0][0].name == "计算机网络");
    CHECK(days[0][0].teacher == "李老师");
    REQUIRE(days[6].size() == 1);
    CHECK(days[6][0].course_id == "42");
    CHECK(days[6][0].live_id == "7");
    CHECK(days[6][0].raw_status == "true");
}

TEST_CASE("parse_live_week_schedule_days 接受字段漂移并跳过空记录", "[LiveParser][contract]") {
    const auto list = nlohmann::json::array({
        nlohmann::json::array({
            nlohmann::json{{"courseId", "course-2"}, {"live_id", "live-2"}, {"courseTitle", "编译原理"}, {"teacherName", "赵老师"}},
            nlohmann::json{{"id", nullptr}, {"course_title", nullptr}, {"teacher_name", "缺少业务字段"}},
        }),
        nlohmann::json{{"courses", nlohmann::json::array({nlohmann::json{{"id", "live-3"}, {"name", "人工智能"}, {"teacher", "钱老师"}}})}},
        nlohmann::json{{"list", nlohmann::json::array({nlohmann::json{{"course_id", "course-4"}, {"name", nlohmann::json::array({"bad"})}}})}},
        nlohmann::json{{"course", "not-array"}},
    });

    auto days = um::Parser::parse_live_week_schedule_days(list);

    REQUIRE(days.size() == 4);
    REQUIRE(days[0].size() == 1);
    CHECK(days[0][0].course_id == "course-2");
    CHECK(days[0][0].live_id == "live-2");
    CHECK(days[0][0].name == "编译原理");
    CHECK(days[0][0].teacher == "赵老师");
    REQUIRE(days[1].size() == 1);
    CHECK(days[1][0].live_id == "live-3");
    CHECK(days[1][0].name == "人工智能");
    REQUIRE(days[2].size() == 1);
    CHECK(days[2][0].course_id == "course-4");
    CHECK(days[2][0].name.empty());
    CHECK(days[3].empty());
}

TEST_CASE("LiveService 查询周课表构造 reference 请求并投影记录", "[service][live]") {
    LiveFixtureHttpClient http_client;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.week_schedule_records({"2026-06-01", "2026-06-07"});

    REQUIRE(result);
    REQUIRE(http_client.request_count == 1);
    CHECK(http_client.last_request.method == um::HttpMethod::Get);
    CHECK(http_client.last_request.url.find("https://yjapi.msa.buaa.edu.cn/courseapi/v2/schedule/get-week-schedules?") == 0);
    CHECK(http_client.last_request.url.find("start_at=2026-06-01") != std::string::npos);
    CHECK(http_client.last_request.url.find("end_at=2026-06-07") != std::string::npos);
    CHECK(http_client.last_request.headers.at("Accept") == "application/json, text/plain, */*");
    CHECK(http_client.last_request.headers.at("Referer") == "https://classroom.msa.buaa.edu.cn/");
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "live-1");
    CHECK((*result)[0].title == "计算机网络");
    CHECK((*result)[0].status == "scheduled");
    CHECK((*result)[0].fields.at("courseId") == "course-1");
    CHECK((*result)[0].fields.at("teacher") == "李老师");
    CHECK((*result)[0].fields.at("day") == "mon");
}

TEST_CASE("LiveService 补齐七天并识别会话失效", "[service][live][session]") {
    LiveFixtureHttpClient ok_client;
    ok_client.body = R"JSON({"success":true,"result":{"code":200,"msg":"","list":[{"course":[]}]}})JSON";
    um::MemoryCacheStore cache;
    um::LiveService service(ok_client, cache, um::ConnectionMode::Direct);

    auto week = service.get_week_schedule({"2026-06-01", "2026-06-07"});

    REQUIRE(week);
    REQUIRE(week->days.size() == 7);

    LiveFixtureHttpClient expired_client;
    expired_client.status_code = 302;
    expired_client.headers["Location"] = "https://sso.buaa.edu.cn/login?service=live";
    expired_client.body.clear();
    um::LiveService expired_service(expired_client, cache, um::ConnectionMode::Direct);

    auto expired = expired_service.get_week_schedule({"2026-06-01", "2026-06-07"});

    REQUIRE_FALSE(expired);
    CHECK(expired.error().code == um::ErrorCode::SessionExpired);
}

TEST_CASE("LiveService 拒绝非法日期且不发请求", "[service][live]") {
    LiveFixtureHttpClient http_client;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.get_week_schedule({"20260601", "2026-06-07"});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
    CHECK(http_client.request_count == 0);
}

TEST_CASE("LiveService 业务失败消息会脱敏", "[service][live][security]") {
    LiveFixtureHttpClient http_client;
    http_client.body = R"JSON({"success":false,"result":{"code":500,"msg":"token=secret-token&Authorization: bearer-secret"}})JSON";
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.get_week_schedule({"2026-06-01", "2026-06-07"});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::NetworkError);
    CHECK(result.error().message.find("secret-token") == std::string::npos);
    CHECK(result.error().message.find("bearer-secret") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);
}
