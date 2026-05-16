#include <UBAANext/Service/JudgeService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <string>

namespace {

constexpr const char *kJudgeCoursesHtml = R"HTML(
<!doctype html>
<html><body>
<a href="courselist.jsp?courseID=1001">程序设计基础</a>
</body></html>
)HTML";

constexpr const char *kJudgeAssignmentsHtml = R"HTML(
<!doctype html>
<html><body>
<table>
<tr><td><a href="assignment/index.jsp?assignID=9001">第一周练习</a></td><td>未提交</td></tr>
<tr><td><a href="assignment/index.jsp?assignID=9002">第二周练习</a></td><td>已过期</td></tr>
<tr><td><a href="assignment/index.jsp?assignID=9003">第三周练习</a></td><td>已提交</td></tr>
</table>
</body></html>
)HTML";

UBAANext::JudgeService make_judge_service(UBAANextMocks::MockHttpClient &http_client, UBAANext::MemoryCacheStore &cache) {
    http_client.set_mock_response("https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F", "<html>judge</html>");
    http_client.set_mock_response("https://judge.buaa.edu.cn/courselist.jsp?courseID=1001", kJudgeCoursesHtml);
    http_client.set_mock_response("https://judge.buaa.edu.cn/assignment/index.jsp", kJudgeAssignmentsHtml);
    return UBAANext::JudgeService(http_client, cache, UBAANext::ConnectionMode::Direct);
}

class JudgeRedirectFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        if (request.url == "https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F") {
            response.status_code = 302;
            response.headers["Location"] = "https://judge.buaa.edu.cn/session/bootstrap";
        } else if (request.url == "https://judge.buaa.edu.cn/session/bootstrap") {
            response.status_code = 302;
            response.headers["Location"] = "/";
        } else if (request.url == "https://judge.buaa.edu.cn/") {
            response.status_code = 200;
            response.body = "<html>judge</html>";
        } else if (request.url == "https://judge.buaa.edu.cn/courselist.jsp?courseID=1001") {
            response.status_code = 302;
            response.headers["Location"] = "courselist-selected.jsp?courseID=1001";
        } else if (request.url == "https://judge.buaa.edu.cn/courselist-selected.jsp?courseID=1001") {
            response.status_code = 200;
            response.body = kJudgeCoursesHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/assignment/index.jsp") {
            response.status_code = 200;
            response.body = kJudgeAssignmentsHtml;
        } else {
            response.status_code = 404;
            response.body = "unexpected url";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
};

} // namespace

TEST_CASE("JudgeService 按 include 参数过滤过期和历史作业", "[service][judge]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    auto service = make_judge_service(http_client, cache);

    UBAANext::JudgeAssignmentQuery query;
    query.course_id = "1001";

    auto current = service.list_assignment_summaries(query);
    REQUIRE(current);
    REQUIRE(current->size() == 1);
    CHECK((*current)[0].id == "9001");
    CHECK((*current)[0].status == "unsubmitted");

    query.include_expired = true;
    auto with_expired = service.list_assignment_summaries(query);
    REQUIRE(with_expired);
    REQUIRE(with_expired->size() == 2);
    CHECK((*with_expired)[0].id == "9001");
    CHECK((*with_expired)[1].id == "9002");
    CHECK((*with_expired)[1].status == "expired");

    query.include_history = true;
    auto with_history = service.list_assignment_summaries(query);
    REQUIRE(with_history);
    REQUIRE(with_history->size() == 3);
    auto submitted = std::find_if(with_history->begin(), with_history->end(), [](const auto &record) {
        return record.id == "9003";
    });
    REQUIRE(submitted != with_history->end());
    CHECK(submitted->status == "submitted");
}

TEST_CASE("JudgeService 跟随会话激活和业务页重定向", "[service][judge]") {
    JudgeRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::JudgeService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    UBAANext::JudgeAssignmentQuery query;
    query.course_id = "1001";
    query.include_expired = true;
    query.include_history = true;

    auto result = service.list_assignment_summaries(query);

    REQUIRE(result);
    REQUIRE(result->size() == 3);
    CHECK(http_client.request_counts["https://judge.buaa.edu.cn/session/bootstrap"] >= 1);
    CHECK(http_client.request_counts["https://judge.buaa.edu.cn/courselist-selected.jsp?courseID=1001"] >= 1);
}
