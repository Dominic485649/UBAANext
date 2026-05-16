#include <UBAANext/Service/SpocService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr const char *kSpocLoginResponse = R"JSON({"code":200,"content":{"jsdm":"student"}})JSON";
constexpr const char *kSpocTermResponse = R"JSON({"code":200,"content":{"mrxq":"2025-2026-2","dqxq":"2025-2026学年第二学期"}})JSON";
constexpr const char *kSpocCoursesResponse = R"JSON({"code":200,"content":[{"kcid":"course-1","kcmc":"智能制造导论","skjs":"张老师"}]})JSON";
constexpr const char *kSpocAssignmentsResponse = R"JSON({"code":200,"content":{"list":[{"zyid":"spoc-1","sskcid":"course-1","zymc":"未交作业","tjzt":"未做"},{"zyid":"spoc-2","sskcid":"course-1","zymc":"过期作业","tjzt":"已过期"},{"zyid":"spoc-3","sskcid":"course-1","zymc":"已交作业","tjzt":"已提交"}],"hasNextPage":false,"pages":1}})JSON";

class SpocFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        UBAANext::HttpResponse response;
        if (request.url == "https://spoc.buaa.edu.cn/spocnewht/cas") {
            response.status_code = 302;
            response.headers["Location"] = "https://spoc.buaa.edu.cn/spocnew/cas?token=test-token";
            return response;
        }
        response.status_code = 200;
        if (request.url == "https://spoc.buaa.edu.cn/spocnewht/sys/casLogin") response.body = kSpocLoginResponse;
        else if (request.url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne") response.body = kSpocTermResponse;
        else if (request.url == "https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=2025-2026-2") response.body = kSpocCoursesResponse;
        else if (request.url == "https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage") response.body = kSpocAssignmentsResponse;
        else response.body = R"JSON({"code":404,"msg":"unexpected url"})JSON";
        return response;
    }
};

UBAANext::SpocService make_spoc_service(SpocFixtureHttpClient &http_client, UBAANext::MemoryCacheStore &cache) {
    return UBAANext::SpocService(http_client, cache, UBAANext::ConnectionMode::Direct);
}

} // namespace

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
    CHECK((*pending_with_expired)[0].id == "spoc-1");
    CHECK((*pending_with_expired)[1].id == "spoc-2");
    CHECK((*pending_with_expired)[1].status == "expired");
}
