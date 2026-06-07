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
constexpr const char *kTopicUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireTopic?id=&rwid=task-1&wjid=questionnaire-1&zdmc=STID&ypjcs=0&xypjcs=1&sxz=&pjrdm=student-1&pjrmc=%E5%AD%A6%E7%94%9F&bpdm=teacher-1&bpmc=%E9%99%88%E8%80%81%E5%B8%88&kcdm=CS101&kcmc=%E7%A8%8B%E5%BA%8F%E8%AE%BE%E8%AE%A1&rwh=task-no&xn=2025&xq=2&xnxq=20252026&pjlxid=3&sfksqbpj=0&yxsfktjst=1&yxdm=";
constexpr const char *kSubmitUrl = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/submitSaveEvaluation";

class EvaluationRedirectFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        ++request_counts[request.url];
        last_bodies[request.url] = request.body;
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
            response.body = R"JSON({"code":1,"data":[{"kcdm":"CS101","kcmc":"程序设计","bpdm":"teacher-1","bpmc":"陈老师","pjrdm":"student-1","pjrmc":"学生","rwh":"task-no","xn":"2025","xq":"2","pjlxid":"3","sfksqbpj":"0","yxsfktjst":"1","ypjcs":0,"xypjcs":1}]})JSON";
        } else if (request.url == kEvaluatedCoursesUrl) {
            response.body = R"JSON({"code":1,"data":[]})JSON";
        } else if (request.url == kTopicUrl) {
            response.body = R"JSON({"code":1,"data":[{"pjxtWjWjbReturnEntity":{"wjzblist":[{"tklist":[{"tmid":"q1","tmlx":"1","tmxxlist":[{"tmxxid":"a1"},{"tmxxid":"a2"}]}]}]},"pjxtPjjgPjjgckb":[{"bprdm":"teacher-1","bprmc":"陈老师","kcdm":"CS101","kcmc":"程序设计","pjfs":"1","pjid":"pj-1","pjlx":"2","pjrdm":"student-1","pjrjsdm":"student","pjrxm":"学生","rwh":"task-no","wjssrwid":"rel-1","xnxq":"20252026","sfxxpj":"1"}],"pjmap":{"key":"value"}}]})JSON";
        } else if (request.url == kSubmitUrl) {
            response.body = submit_response;
        } else {
            response.status_code = 404;
            response.body = R"JSON({"code":0,"msg":"unexpected url"})JSON";
        }
        return response;
    }

    std::map<std::string, int> request_counts;
    std::map<std::string, std::string> last_bodies;
    std::string submit_response = R"JSON({"code":200,"msg":"ok","data":{}})JSON";
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
    CHECK((*result)[0].evaluator_code == "student-1");
    CHECK((*result)[0].evaluator_name == "学生");
    CHECK((*result)[0].assignment_no == "task-no");
    CHECK((*result)[0].evaluation_type_id == "3");
    CHECK((*result)[0].allow_all == "0");
    CHECK((*result)[0].department_submit_status == "1");
    CHECK(http_client.request_counts[kActivationBootstrapUrl] == 1);
    CHECK(http_client.request_counts[kTaskRedirectUrl] == 1);
}

TEST_CASE("EvaluationService 提交携带课程上下文字段", "[service][evaluation]") {
    EvaluationRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::EvaluationService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "evaluation submit"));

    auto result = service.submit_evaluations("CS101");

    REQUIRE(result);
    CHECK(result->accepted);
    CHECK(http_client.request_counts[kTopicUrl] == 1);
    CHECK(http_client.request_counts[kSubmitUrl] == 1);
    REQUIRE(http_client.last_bodies.count(kSubmitUrl) == 1);
    auto body = nlohmann::json::parse(http_client.last_bodies[kSubmitUrl]);
    CHECK(body["pjjglist"][0]["pjrdm"] == "student-1");
    CHECK(body["pjjglist"][0]["rwh"] == "task-no");
    CHECK(body["pjjglist"][0]["pjxxlist"][0]["xxdalist"].size() == 1);
}

TEST_CASE("EvaluationService 获取表单摘要", "[service][evaluation]") {
    EvaluationRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::EvaluationService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto result = service.form_record("CS101");

    REQUIRE(result);
    CHECK(result->id == "task-1_questionnaire-1_CS101_teacher-1");
    CHECK(result->fields.at("questionCount") == "1");
    CHECK(result->fields.at("formResultId") == "pj-1");
    CHECK(http_client.request_counts[kTopicUrl] == 1);
}

TEST_CASE("EvaluationService 指定表单提交复用默认填充", "[service][evaluation]") {
    EvaluationRedirectFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::EvaluationService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "evaluation form submit"));

    UBAANext::Model::EvaluationSubmission submission;
    submission.target_id = "task-1_questionnaire-1_CS101_teacher-1";
    auto result = service.submit_form(submission);

    REQUIRE(result);
    CHECK(result->accepted);
    REQUIRE(http_client.last_bodies.count(kSubmitUrl) == 1);
    auto body = nlohmann::json::parse(http_client.last_bodies[kSubmitUrl]);
    CHECK(body["pjjglist"][0]["kcdm"] == "CS101");
    CHECK(body["pjjglist"][0]["pjxxlist"][0]["xxdalist"][0] == "a2");
}

TEST_CASE("EvaluationService 提交识别业务失败", "[service][evaluation]") {
    EvaluationRedirectFixtureHttpClient http_client;
    http_client.submit_response = R"JSON({"code":500,"msg":"已经评教","data":{}})JSON";
    UBAANext::MemoryCacheStore cache;
    UBAANext::EvaluationService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "evaluation submit"));

    auto result = service.submit_evaluations("CS101");

    REQUIRE(result);
    CHECK_FALSE(result->accepted);
    CHECK(result->message == "评教提交完成: 0/1");
}
