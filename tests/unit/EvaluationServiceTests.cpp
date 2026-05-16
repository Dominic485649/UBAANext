#include <UBAANext/Service/EvaluationService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

namespace {

constexpr const char *kActivationUrl = "https://spoc.buaa.edu.cn/pjxt/cas";
constexpr const char *kActivationBootstrapUrl = "https://spoc.buaa.edu.cn/pjxt/bootstrap";
constexpr const char *kTermUrl = "https://spoc.buaa.edu.cn/pjxt/component/queryXnxq";
constexpr const char *kTaskUrl = "https://spoc.buaa.edu.cn/pjxt/personnelEvaluation/listObtainPersonnelEvaluationTasks?rwmc=&sfyp=0&pageNum=1&pageSize=10";
constexpr const char *kTaskRedirectUrl = "https://spoc.buaa.edu.cn/pjxt/personnelEvaluation/listObtainPersonnelEvaluationTasksRedirect";
constexpr const char *kQuestionnairesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireListToTask?rwid=task-1&sfyp=0&pageNum=1&pageSize=999";
constexpr const char *kReviseUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern";
constexpr const char *kPendingCoursesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=0&wjid=questionnaire-1&xnxq=20252026&pageNum=1&pageSize=999";
constexpr const char *kEvaluatedCoursesUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=1&wjid=questionnaire-1&xnxq=20252026&pageNum=1&pageSize=999";

class EvaluationRedirectFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == kActivationUrl) {
            response.status_code = 302;
            response.headers["Location"] = "/pjxt/bootstrap";
        } else if (request.url == kActivationBootstrapUrl) {
            response.body = R"JSON({"code":1,"data":{}})JSON";
        } else if (request.url == kTermUrl) {
            response.body = R"JSON({"code":1,"data":[{"xn":"2025","xq":"2026"}]})JSON";
        } else if (request.url == kTaskUrl) {
            response.status_code = 302;
            response.headers["Location"] = "listObtainPersonnelEvaluationTasksRedirect";
        } else if (request.url == kTaskRedirectUrl) {
            response.body = R"JSON({"code":1,"data":{"list":[{"rwid":"task-1"}]}})JSON";
        } else if (request.url == kQuestionnairesUrl) {
            response.body = R"JSON({"code":1,"data":[{"wjid":"questionnaire-1","msid":"1"}]})JSON";
        } else if (request.url == kReviseUrl) {
            response.body = R"JSON({"code":1,"data":{}})JSON";
        } else if (request.url == kPendingCoursesUrl) {
            response.body = R"JSON({"code":1,"data":[{"kcdm":"CS101","kcmc":"程序设计","bpdm":"teacher-1","bpmc":"陈老师","ypjcs":0,"xypjcs":1}]})JSON";
        } else if (request.url == kEvaluatedCoursesUrl) {
            response.body = R"JSON({"code":1,"data":[]})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"code":0,"msg":"unexpected url"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
};

} // namespace

TEST_CASE("EvaluationService 跟随会话激活和列表重定向", "[service][evaluation]") {
    EvaluationRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::EvaluationService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.list_evaluation_tasks();

    REQUIRE(result);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "task-1_questionnaire-1_CS101_teacher-1");
    CHECK((*result)[0].status == "pending");
    CHECK(http_client.request_counts[kActivationBootstrapUrl] == 1);
    CHECK(http_client.request_counts[kTaskRedirectUrl] == 1);
}
