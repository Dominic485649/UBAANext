#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

constexpr const char *kSpocLoginResponse = R"JSON({"code":200,"content":{"jsdm":"student"}})JSON";
constexpr const char *kSpocTermResponse = R"JSON({"code":200,"content":{"mrxq":"2025-2026-2","dqxq":"2025-2026学年第二学期"}})JSON";
constexpr const char *kSpocWeekResponse = R"JSON({"code":200,"content":{"mrxq":"2025-2026-2","pjmrrq":"1,2026-06-01,2026-06-07"}})JSON";
constexpr const char *kSpocCoursesResponse = R"JSON({"code":200,"content":[{"kcid":"course-1","kcmc":"智能制造导论","skjs":"张老师"}]})JSON";
constexpr const char *kSpocScheduleResponse = R"JSON({"code":200,"content":[{"id":"schedule-1","kcid":"course-1","kcmc":"智能制造导论","jsxm":"张老师","weekday":"monday","skdd":"主M101","kcsj":"2026-06-01 08:00-09:35"}]})JSON";
constexpr const char *kSpocAssignmentsResponse = R"JSON({"code":200,"content":{"list":[{"zyid":"spoc-3","sskcid":"course-1","zymc":"已交作业","tjzt":"已提交","zyjzsj":"2026-03-20 23:59:00"},{"zyid":"spoc-2","sskcid":"course-1","zymc":"过期作业","tjzt":"已过期","zyjzsj":"2026-03-08 23:59:00"},{"zyid":"spoc-1","sskcid":"course-1","zymc":"未交作业","tjzt":"未做","zyjzsj":"2026-03-10 23:59:00"}],"hasNextPage":false,"pages":1}})JSON";
constexpr const char *kSpocDetailResponse = R"JSON({"code":200,"content":{"sskcid":"course-1","zymc":"单详情作业","zykssj":"2026-03-01 08:00:00","zyjzsj":"2026-03-10 23:59:00","zyfs":"100","zynr":"<p>完成报告</p>"}})JSON";
constexpr const char *kSpocSubmissionResponse = R"JSON({"code":200,"content":{"tjzt":"已提交","tjsj":"2026-03-09 20:00:00"}})JSON";
constexpr const char *kSpocSubmitResponse = R"JSON({"code":200,"msg":"操作成功","msg_en":"操作成功","content":null})JSON";
constexpr const char *kSpocSubmissionErrorResponse = R"JSON({"code":500,"msg":"Cookie: SID=cookie-secret&photo_path=C:/secret/spoc-submit.html","content":null})JSON";
constexpr const char *kSpocDetailErrorResponse = R"JSON({"code":500,"msg":"captcha=captcha-secret&Authorization: bearer-secret&photo_path=C:/secret/spoc-detail.html","content":null})JSON";

std::string direct_spoc_url(const std::string &url) {
    const auto prefix = std::string("https://d.buaa.edu.cn/https/");
    auto pos = url.find("/spocnew", prefix.size());
    return pos == std::string::npos ? url : "https://spoc.buaa.edu.cn" + url.substr(pos);
}

class SpocFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_total;
        requests.push_back(request);
        const auto url = direct_spoc_url(request.url);
        UBAANext::HttpResponse response;
        if (url == "https://spoc.buaa.edu.cn/spocnewht/cas") {
            response.status_code = 302;
            response.headers["Location"] = "https://spoc.buaa.edu.cn/spocnew/cas?token=test-token";
            return response;
        }
        response.status_code = 200;
        if (url == "https://spoc.buaa.edu.cn/spocnewht/sys/casLogin") response.body = kSpocLoginResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne" && request.body.find("17275975753144ed8d6fe15425677f752c936d97de1bab76") != std::string::npos) response.body = kSpocWeekResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne") response.body = kSpocTermResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryRlData?rllx=1&zksrq=2026-06-01&zjsrq=2026-06-07") response.body = kSpocScheduleResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?xnxq=2025-2026-2") response.body = kSpocCoursesResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=2025-2026-2") response.body = kSpocCoursesResponse;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage") response.body = kSpocAssignmentsResponse;
        else if (url.find("https://spoc.buaa.edu.cn/spocnewht/kczy/queryKczyInfoByid?id=") == 0) response.body = detail_body;
        else if (url.find("https://spoc.buaa.edu.cn/spocnewht/kczy/queryXsSubmitKczyInfo?kczyid=") == 0) response.body = submission_body;
        else if (url == "https://spoc.buaa.edu.cn/spocnewht/kczy/submitKcz2") response.body = kSpocSubmitResponse;
        else response.body = R"JSON({"code":404,"msg":"unexpected url"})JSON";
        return response;
    }
    std::string detail_body = kSpocDetailResponse;
    std::string submission_body = kSpocSubmissionResponse;
    int request_total = 0;
    std::vector<UBAANext::HttpRequest> requests;
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
        if (request.url.find("sso.buaa.edu.cn/login") != std::string::npos || (request.url.find("/https/") != std::string::npos && request.url.find("/login") != std::string::npos)) {
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

TEST_CASE("SpocService 单详情返回详情和提交状态", "[service][spoc][contract]") {
    SpocFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto detail = service.assignment_detail("spoc-1");

    REQUIRE(detail);
    CHECK(detail->id == "spoc-1");
    CHECK(detail->course_id == "course-1");
    CHECK(detail->title == "单详情作业");
    CHECK(detail->status == "submitted");
    CHECK(detail->submitted_at == "2026-03-09 20:00:00");
    CHECK(detail->content == "完成报告");

    auto record = service.show_assignment("spoc-1");
    REQUIRE(record);
    CHECK(record->id == "spoc-1");
    CHECK(record->status == "submitted");
    CHECK(record->fields.at("content") == "完成报告");
    CHECK(record->fields.at("submissionStatus") == "已提交");
}

TEST_CASE("SpocService 单详情提交信息失败时保留详情并降级为 unknown", "[service][spoc][contract][redaction]") {
    SpocFixtureHttpClient http_client;
    http_client.submission_body = kSpocSubmissionErrorResponse;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto detail = service.assignment_detail("spoc-1");

    REQUIRE(detail);
    CHECK(detail->id == "spoc-1");
    CHECK(detail->status == "unknown");
    CHECK(detail->submission_status.empty());
    CHECK(detail->submitted_at.empty());
    CHECK(detail->content == "完成报告");
}

TEST_CASE("SpocService 单详情业务错误消息会脱敏", "[service][spoc][security]") {
    SpocFixtureHttpClient http_client;
    http_client.detail_body = kSpocDetailErrorResponse;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto detail = service.assignment_detail("spoc-1");

    REQUIRE_FALSE(detail);
    CHECK(detail.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(detail.error().message.find("captcha-secret") == std::string::npos);
    CHECK(detail.error().message.find("bearer-secret") == std::string::npos);
    CHECK(detail.error().message.find("C:/secret/spoc-detail.html") == std::string::npos);
    CHECK(detail.error().message.find("[REDACTED]") != std::string::npos);
}

TEST_CASE("SpocService 单详情登录页返回 SessionExpired", "[service][spoc][contract]") {
    SpocFixtureHttpClient http_client;
    http_client.detail_body = "<html><body>统一身份认证<input name=\"execution\" value=\"login-ticket\"></body></html>";
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto detail = service.assignment_detail("spoc-1");

    REQUIRE_FALSE(detail);
    CHECK(detail.error().code == UBAANext::ErrorCode::SessionExpired);
}

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

TEST_CASE("SpocService 对齐 buaa-api 周次课表和课程接口", "[service][spoc][migration]") {
    SpocFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto week = service.current_week();
    REQUIRE(week);
    CHECK(week->term_code == "2025-2026-2");
    CHECK(week->start_date == "2026-06-01");
    CHECK(week->end_date == "2026-06-07");

    auto schedules = service.week_schedule(week->start_date, week->end_date);
    REQUIRE(schedules);
    REQUIRE(schedules->size() == 1);
    CHECK((*schedules)[0].id == "schedule-1");
    CHECK((*schedules)[0].course_id == "course-1");
    CHECK((*schedules)[0].start_time == "2026-06-01 08:00");
    CHECK((*schedules)[0].end_time == "2026-06-01 09:35");

    auto courses = service.courses(week->term_code);
    REQUIRE(courses);
    REQUIRE(courses->size() == 1);
    CHECK((*courses)[0].id == "course-1");
    CHECK((*courses)[0].name == "智能制造导论");
}

TEST_CASE("SpocService 作业提交缺少确认不发请求", "[service][spoc][write-gate]") {
    SpocFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);

    auto result = service.submit_homework({"spoc-1", "course-1", "file-1", "report.pdf"});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.request_total == 0);
}

TEST_CASE("SpocService 作业提交发送 reference 表单字段", "[service][spoc][write-gate]") {
    SpocFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_spoc_service(http_client, cache);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "spoc homework submit"));

    auto result = service.submit_homework({"spoc-1", "course-1", "file-1", "report.pdf"});

    REQUIRE(result);
    CHECK(result->accepted);
    REQUIRE_FALSE(http_client.requests.empty());
    const auto &request = http_client.requests.back();
    CHECK(direct_spoc_url(request.url) == "https://spoc.buaa.edu.cn/spocnewht/kczy/submitKcz2");
    CHECK(request.body.find("ytjcs=2") != std::string::npos);
    CHECK(request.body.find("tjfs=5") != std::string::npos);
    CHECK(request.body.find("sskcid=course-1") != std::string::npos);
    CHECK(request.body.find("kczyid=spoc-1") != std::string::npos);
    CHECK(request.body.find("scwjid=file-1") != std::string::npos);
    CHECK(request.body.find("scwjid_name=report.pdf") != std::string::npos);
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
