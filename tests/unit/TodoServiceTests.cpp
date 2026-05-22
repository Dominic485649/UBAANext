#include <UBAANext/Service/TodoService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

namespace {

constexpr const char *kSpocLoginResponse = R"JSON({"code":200,"content":{"jsdm":"student"}})JSON";
constexpr const char *kSpocTermResponse = R"JSON({"code":200,"content":{"mrxq":"2025-2026-2","dqxq":"2025-2026学年第二学期"}})JSON";
constexpr const char *kSpocCoursesResponse = R"JSON({"code":200,"content":[{"kcid":"course-1","kcmc":"智能制造导论","skjs":"张老师"}]})JSON";
constexpr const char *kSpocAssignmentsResponse = R"JSON({"code":200,"content":{"list":[{"zyid":"spoc-1","sskcid":"course-1","zymc":"未交作业","tjzt":"未做"},{"zyid":"spoc-2","sskcid":"course-1","zymc":"过期作业","tjzt":"已过期"},{"zyid":"spoc-3","sskcid":"course-1","zymc":"已交作业","tjzt":"已提交"}],"hasNextPage":false,"pages":1}})JSON";
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
constexpr const char *kJudgeDetailUnsubmittedHtml = R"HTML(
<!doctype html>
<html><body>
<div>作业时间：2026-03-10 08:00 至 2026-03-20 23:59</div>
<div>作业满分：100</div>
<div>共 1 道题</div>
<article>题目一 未提交</article>
</body></html>
)HTML";
constexpr const char *kJudgeDetailExpiredHtml = R"HTML(
<!doctype html>
<html><body>
<div>作业时间：2026-02-01 08:00 至 2026-02-08 23:59</div>
<div>作业满分：100</div>
<div>共 1 道题</div>
<article>题目一 未提交</article>
</body></html>
)HTML";
constexpr const char *kJudgeDetailSubmittedHtml = R"HTML(
<!doctype html>
<html><body>
<div>作业时间：2026-01-01 08:00 至 2026-01-08 23:59</div>
<div>作业满分：100</div>
<div>共 1 道题</div>
<article>题目一 得分：100</article>
<div>总分：100</div>
</body></html>
)HTML";
constexpr const char *kEvaluationTermUrl = "https://spoc.buaa.edu.cn/pjxt/component/queryXnxq";
constexpr const char *kEvaluationTasksUrl = "https://spoc.buaa.edu.cn/pjxt/personnelEvaluation/listObtainPersonnelEvaluationTasks?rwmc=&sfyp=0&pageNum=1&pageSize=10";
constexpr const char *kEvaluationQuestionnairesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireListToTask?rwid=task-1&sfyp=0&pageNum=1&pageSize=999";
constexpr const char *kEvaluationReviseUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern";
constexpr const char *kEvaluationPendingCoursesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=0&wjid=questionnaire-1&xnxq=20252026&pageNum=1&pageSize=999";
constexpr const char *kEvaluationEvaluatedCoursesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=1&wjid=questionnaire-1&xnxq=20252026&pageNum=1&pageSize=999";

std::string today_yyyymmdd() {
    auto now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d");
    return out.str();
}

std::string signin_today_url() {
    return "https://iclass.buaa.edu.cn:8347/app/course/get_stu_course_sched.action?id=user-1&dateStr=" + today_yyyymmdd();
}

std::string direct_spoc_url(const std::string &url) {
    const auto prefix = std::string("https://d.buaa.edu.cn/https/");
    auto pos = url.find("/spocnew", prefix.size());
    if (pos == std::string::npos) {
        pos = url.find("/pjxt", prefix.size());
    }
    return pos == std::string::npos ? url : "https://spoc.buaa.edu.cn" + url.substr(pos);
}

class TodoFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        const auto url = direct_spoc_url(request.url);
        ++request_counts[url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (url == "https://spoc.buaa.edu.cn/spocnewht/cas") {
            response.status_code = 302;
            response.headers["Location"] = "https://spoc.buaa.edu.cn/spocnew/cas?token=test-token";
        } else if (url == "https://spoc.buaa.edu.cn/spocnewht/sys/casLogin") {
            response.body = kSpocLoginResponse;
        } else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne") {
            response.body = kSpocTermResponse;
        } else if (url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=2025-2026-2") {
            response.body = kSpocCoursesResponse;
        } else if (url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage") {
            response.body = kSpocAssignmentsResponse;
        } else if (request.url == "https://sso.buaa.edu.cn/login?service=http%3A%2F%2Fjudge.buaa.edu.cn%2F") {
            response.body = "<html>judge</html>";
        } else if (request.url == "https://judge.buaa.edu.cn/courselist.jsp?courseID=0") {
            response.body = kJudgeCoursesHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/courselist.jsp?courseID=1001") {
            response.body = kJudgeCoursesHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/assignment/index.jsp") {
            response.body = kJudgeAssignmentsHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/assignment/index.jsp?assignID=9001") {
            response.body = kJudgeDetailUnsubmittedHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/assignment/index.jsp?assignID=9002") {
            response.body = kJudgeDetailExpiredHtml;
        } else if (request.url == "https://judge.buaa.edu.cn/assignment/index.jsp?assignID=9003") {
            response.body = kJudgeDetailSubmittedHtml;
        } else if (request.url == "https://uc.buaa.edu.cn/api/uc/userinfo") {
            response.body = R"JSON({"code":0,"data":{"schoolid":"20260000"}})JSON";
        } else if (request.url == "https://iclass.buaa.edu.cn:8347/app/user/login.action?password=&phone=20260000&userLevel=1&verificationType=2&verificationUrl=") {
            response.body = R"JSON({"STATUS":"0","result":{"id":"user-1","sessionId":"session-1"}})JSON";
        } else if (request.url == signin_today_url()) {
            response.body = R"JSON({"STATUS":0,"result":[{"id":"signin-1","courseName":"今日签到","signStatus":"0"},{"id":"signin-2","courseName":"已签到课程","signStatus":"1"}]})JSON";
        } else if (url == "https://spoc.buaa.edu.cn/pjxt/cas") {
            response.body = R"JSON({"code":1,"data":{}})JSON";
        } else if (url == kEvaluationTermUrl) {
            response.body = R"JSON({"code":1,"data":[{"xn":"2025","xq":"2026"}]})JSON";
        } else if (url == kEvaluationTasksUrl) {
            response.body = R"JSON({"code":1,"data":{"list":[{"rwid":"task-1"}]}})JSON";
        } else if (url == kEvaluationQuestionnairesUrl) {
            response.body = R"JSON({"code":1,"data":[{"wjid":"questionnaire-1","msid":"1"}]})JSON";
        } else if (url == kEvaluationReviseUrl) {
            response.body = R"JSON({"code":1,"data":{}})JSON";
        } else if (url == kEvaluationPendingCoursesUrl) {
            response.body = R"JSON({"code":1,"data":[{"kcdm":"CS101","kcmc":"程序设计","bpdm":"teacher-1","bpmc":"陈老师","ypjcs":0,"xypjcs":1}]})JSON";
        } else if (url == kEvaluationEvaluatedCoursesUrl) {
            response.body = R"JSON({"code":1,"data":[{"kcdm":"CS102","kcmc":"已评课程","bpdm":"teacher-2","bpmc":"王老师","ypjcs":1,"xypjcs":1}]})JSON";
        } else {
            response.status_code = 404;
            response.body = "{}";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
};

} // namespace

TEST_CASE("TodoService 聚合 Core 待办并规范字段", "[service][todo]") {
    TodoFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::TodoService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list_todos();

    REQUIRE(result);
    REQUIRE(result->size() == 4);
    std::map<std::string, UBAANext::Model::FeatureRecord> records;
    for (const auto &record : *result) records[record.fields.at("source") + ":" + record.id] = record;
    REQUIRE(records.count("spoc:spoc-1") == 1);
    REQUIRE(records.count("judge:9001") == 1);
    REQUIRE(records.count("signin:signin-1") == 1);
    REQUIRE(records.count("evaluation:task-1_questionnaire-1_CS101_teacher-1") == 1);
    CHECK(records["spoc:spoc-1"].status == "unsubmitted");
    CHECK(records["judge:9001"].fields.at("submissionStatus") == "unsubmitted");
    CHECK(records["signin:signin-1"].fields.at("type") == "signin");
}

TEST_CASE("TodoService 可返回非待办聚合记录", "[service][todo]") {
    TodoFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::TodoService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::TodoQuery query;
    query.pending_only = false;

    auto result = service.list_todos(query);

    REQUIRE(result);
    REQUIRE(result->size() == 7);
}
