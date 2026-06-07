#include <UBAANext/Parser/LiveParser.hpp>
#include <UBAANext/Service/LiveService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace um = UBAANext;

namespace
{

class LiveFixtureHttpClient : public um::IHttpClient
{
  public:
    um::Result<um::HttpResponse> send(const um::HttpRequest &request) override
    {
        ++request_count;
        last_request = request;
        if (!responses.empty())
        {
            auto response = std::move(responses.front());
            responses.erase(responses.begin());
            return response;
        }
        um::HttpResponse response;
        response.status_code = status_code;
        response.headers = headers;
        response.body = body;
        return response;
    }

    int request_count = 0;
    int status_code = 200;
    std::unordered_map<std::string, std::string> headers;
    std::string body =
        R"JSON({"success":true,"result":{"code":200,"msg":"","list":[{"course":[{"course_id":"course-1","id":"live-1","course_title":"计算机网络","teacher_name":"李老师"}]},{"course":[]},{"course":[]},{"course":[]},{"course":[]},{"course":[]},{"course":[]}]}})JSON";
    um::HttpRequest last_request;
    std::vector<um::HttpResponse> responses;
};

class LiveFixtureCookieStore : public um::ICookieStore
{
  public:
    um::Result<um::CookieJar> load() override { return jar; }

    um::Result<void> save(const um::CookieJar &cookies) override
    {
        jar = cookies;
        return {};
    }

    um::Result<void> save_current() override { return {}; }

    um::Result<void> clear() override
    {
        jar.clear();
        return {};
    }

    const um::CookieJar *current() const override { return &jar; }

    um::CookieJar jar;
};

} // namespace

TEST_CASE("parse_live_week_schedule_days 解析 reference 周课表 envelope list", "[LiveParser]")
{
    const auto list = nlohmann::json::array({
        nlohmann::json{
            {"course", nlohmann::json::array({nlohmann::json{{"course_id", "course-1"},
                                                             {"id", "live-1"},
                                                             {"course_title", "计算机网络"},
                                                             {"teacher_name", "李老师"}}})}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{{"course", nlohmann::json::array()}},
        nlohmann::json{
            {"course", nlohmann::json::array({nlohmann::json{{"course_id", 42},
                                                             {"id", 7},
                                                             {"course_title", "周日课程"},
                                                             {"teacher_name", "王老师"},
                                                             {"status", true}}})}},
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

TEST_CASE("parse_live_week_schedule_days 接受字段漂移并跳过空记录", "[LiveParser][contract]")
{
    const auto list = nlohmann::json::array({
        nlohmann::json::array({
            nlohmann::json{{"courseId", "course-2"},
                           {"live_id", "live-2"},
                           {"courseTitle", "编译原理"},
                           {"teacherName", "赵老师"}},
            nlohmann::json{
                {"id", nullptr}, {"course_title", nullptr}, {"teacher_name", "缺少业务字段"}},
        }),
        nlohmann::json{
            {"courses", nlohmann::json::array({nlohmann::json{
                            {"id", "live-3"}, {"name", "人工智能"}, {"teacher", "钱老师"}}})}},
        nlohmann::json{
            {"list", nlohmann::json::array({nlohmann::json{
                         {"course_id", "course-4"}, {"name", nlohmann::json::array({"bad"})}}})}},
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

TEST_CASE("LiveService 查询周课表构造 reference 请求并投影记录", "[service][live]")
{
    LiveFixtureHttpClient http_client;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.week_schedule_records({"2026-06-01", "2026-06-07"});

    REQUIRE(result);
    REQUIRE(http_client.request_count == 1);
    CHECK(http_client.last_request.method == um::HttpMethod::Get);
    CHECK(http_client.last_request.url.find(
              "https://yjapi.msa.buaa.edu.cn/courseapi/v2/schedule/get-week-schedules?") == 0);
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

TEST_CASE("LiveService 补齐七天并识别会话失效", "[service][live][session]")
{
    LiveFixtureHttpClient ok_client;
    ok_client.body =
        R"JSON({"success":true,"result":{"code":200,"msg":"","list":[{"course":[]}]}})JSON";
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

TEST_CASE("LiveService 拒绝非法日期且不发请求", "[service][live]")
{
    LiveFixtureHttpClient http_client;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.get_week_schedule({"20260601", "2026-06-07"});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
    CHECK(http_client.request_count == 0);
}

TEST_CASE("LiveService 业务失败消息会脱敏", "[service][live][security]")
{
    LiveFixtureHttpClient http_client;
    http_client.body =
        R"JSON({"success":false,"result":{"code":500,"msg":"token=secret-token&Authorization: bearer-secret"}})JSON";
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, cache, um::ConnectionMode::Direct);

    auto result = service.get_week_schedule({"2026-06-01", "2026-06-07"});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::NetworkError);
    CHECK(result.error().message.find("secret-token") == std::string::npos);
    CHECK(result.error().message.find("bearer-secret") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);
}

TEST_CASE("LiveService resources 从 _token cookie 添加 Bearer token", "[service][live][token]")
{
    LiveFixtureHttpClient http_client;
    http_client.body =
        R"JSON({"code":0,"result":{"list":[{"course_id":"course-1","sub_id":"sub-1","title":"计算机网络","sub_status":6}]}})JSON";
    LiveFixtureCookieStore cookies;
    const std::string jwt = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig_123";
    cookies.jar.set_cookie("msa.buaa.edu.cn", "_token",
                           "a%3A2%3A%7Bi%3A0%3Bs%3A6%3A%22_token%22%3Bi%3A1%3Bs%3A52%3A%22" + jwt +
                               "%22%3B%7D");
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, &cookies, cache, um::ConnectionMode::Direct);

    um::Model::LiveResourceQuery query;
    query.date = "2026-06-08";
    auto result = service.resources(query);

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    REQUIRE(http_client.last_request.headers.count("Authorization") == 1);
    CHECK(http_client.last_request.headers.at("Authorization") == "Bearer " + jwt);
}

TEST_CASE("LiveService resources 可从 live 激活响应提取 Bearer token", "[service][live][token]")
{
    LiveFixtureHttpClient http_client;
    const std::string jwt = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig_456";
    http_client.body =
        R"JSON({"code":0,"token":")JSON" + jwt +
        R"JSON(","result":{"list":[{"course_id":"course-1","sub_id":"sub-1","title":"计算机网络","sub_status":6}]}})JSON";
    LiveFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, &cookies, cache, um::ConnectionMode::Direct);

    um::Model::LiveResourceQuery query;
    query.date = "2026-06-08";
    auto result = service.resources(query);

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK(http_client.request_count == 2);
    REQUIRE(http_client.last_request.headers.count("Authorization") == 1);
    CHECK(http_client.last_request.headers.at("Authorization") == "Bearer " + jwt);
}

TEST_CASE("LiveService PPT 时间轴复用 live 激活响应 Bearer token", "[service][live][token]")
{
    LiveFixtureHttpClient http_client;
    const std::string jwt = "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxIn0.sig_789";
    um::HttpResponse login;
    login.status_code = 200;
    login.body = R"JSON({"code":0,"token":")JSON" + jwt + R"JSON("})JSON";
    um::HttpResponse slides;
    slides.status_code = 200;
    slides.body = R"JSON({"code":0,"result":{"list":[{"created_sec":10,"img_url":"https://media.example/slide1.jpg"}]}})JSON";
    http_client.responses = {login, slides};

    LiveFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    um::LiveService service(http_client, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.ppt_slides("course-1", "sub-1", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK(http_client.request_count == 2);
    REQUIRE(http_client.last_request.headers.count("Authorization") == 1);
    CHECK(http_client.last_request.headers.at("Authorization") == "Bearer " + jwt);
}

TEST_CASE("parse_live_resources 映射 BBUAA 课堂资源和 sub_status", "[LiveParser][live-resource]")
{
    const auto root = nlohmann::json::parse(R"JSON({
        "result": {"list": [{
            "name": "1-2",
            "class_begin_time": "08:00",
            "class_end_time": "09:40",
            "list": [{
                "course_id": "course-1",
                "sub_id": "sub-1",
                "title": "计算机网络",
                "course_code": "CS101",
                "lecturer_name": "李老师",
                "room_name": "J3-101",
                "sub_status": 6
            }]
        }]}
    })JSON");

    const auto resources = um::Parser::parse_live_resources(root, "bbuaa");

    REQUIRE(resources.size() == 1);
    CHECK(resources[0].course_id == "course-1");
    CHECK(resources[0].sub_id == "sub-1");
    CHECK(resources[0].title == "计算机网络");
    CHECK(resources[0].status_label == "回放");
    CHECK(resources[0].time_slot == "1-2");
    CHECK(resources[0].time_range == "08:00-09:40");
    CHECK(resources[0].source == "bbuaa");
}

TEST_CASE("parse_live_resource_detail 解析视频 URL 与 PPT GUID 候选", "[LiveParser][live-resource]")
{
    const auto item = nlohmann::json::parse(R"JSON({
        "course_id": "course-1",
        "sub_id": "sub-1",
        "title": "计算机网络",
        "sub_status": 7,
        "sub_resource_guid": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "sub_content": "{\"save_playback\":{\"contents\":\"https://media.example/video.mp4\",\"is_m3u8\":\"no\"},\"output\":{\"m3u8\":\"https://media.example/live.m3u8\"}}",
        "video_list": [
            {"type": "2", "resource_guid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "preview_url": "https://media.example/ppt.mp4"},
            {"type": "3", "resource_guid": "cccccccccccccccccccccccccccccccc", "preview_url": "https://media.example/teacher.mp4"}
        ]
    })JSON");

    const auto detail = um::Parser::parse_live_resource_detail(item);

    CHECK(detail.course_id == "course-1");
    CHECK(detail.sub_id == "sub-1");
    CHECK(detail.status_label == "回放");
    CHECK(detail.playback_url == "https://media.example/video.mp4");
    CHECK(detail.live_url == "https://media.example/live.m3u8");
    CHECK(detail.primary_video_hls);
    CHECK(detail.has_video);
    CHECK(detail.ppt_resource_guid == "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(detail.sub_resource_guid == "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    REQUIRE(detail.ppt_guids.size() >= 2);
}

TEST_CASE("parse_live_livingroom_html 抽取 HTML/JS 视频与 GUID", "[LiveParser][live-resource]")
{
    const std::string html = R"HTML(
        <html><body>
        <video src="https://media.example/replay.mp4"></video>
        <script>
          window.__play = { streamUrl: "https://media.example/live.m3u8", resource_guid: "dddddddddddddddddddddddddddddddd" };
        </script>
        </body></html>
    )HTML";

    const auto detail = um::Parser::parse_live_livingroom_html(html);

    CHECK(detail.has_video);
    CHECK(detail.live_url == "https://media.example/live.m3u8");
    CHECK(detail.playback_url == "https://media.example/replay.mp4");
    REQUIRE_FALSE(detail.ppt_guids.empty());
    CHECK(detail.ppt_guids[0] == "dddddddddddddddddddddddddddddddd");
}

TEST_CASE("parse_live_ppt_slides 解析时间轴并排序", "[LiveParser][ppt]")
{
    const auto root = nlohmann::json::parse(R"JSON({
        "data": {"list": [
            {"created_sec": 20, "content": "{\"pptimgurl\":\"https://media.example/slide2.jpg\"}"},
            {"created_sec": 10, "img_url": "https://media.example/slide1.png"}
        ]}
    })JSON");

    const auto slides = um::Parser::parse_live_ppt_slides(root);

    REQUIRE(slides.size() == 2);
    CHECK(slides[0].time_sec == 10);
    CHECK(slides[0].index == 0);
    CHECK(slides[0].image_url == "https://media.example/slide1.png");
    CHECK(slides[1].time_sec == 20);
}

TEST_CASE("build_live_pptx 生成可识别 ZIP/OOXML 结构", "[LiveParser][pptx]")
{
    um::Model::LiveBinaryResource image;
    image.name = "slide1.jpg";
    image.content_type = "image/jpeg";
    image.bytes = {0xFF, 0xD8, 0xFF, 0xD9};

    const auto pptx = um::Parser::build_live_pptx({image});
    const std::string bytes(pptx.begin(), pptx.end());

    REQUIRE(pptx.size() > 100);
    CHECK(pptx[0] == 'P');
    CHECK(pptx[1] == 'K');
    CHECK(bytes.find("[Content_Types].xml") != std::string::npos);
    CHECK(bytes.find("ppt/slides/slide1.xml") != std::string::npos);
    CHECK(bytes.find("ppt/media/image1.jpg") != std::string::npos);
}
