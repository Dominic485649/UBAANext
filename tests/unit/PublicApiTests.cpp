#include <UBAANext/UBAANext.hpp>
#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>
#include <UBAANextMocks/MockSecureStore.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("伞头暴露 Core SDK 常用入口", "[public-api]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockCacheStore cache;
    UBAANextMocks::MockSecureStore secure_store;

    UBAANext::AuthService auth_service(http_client, secure_store);
    UBAANext::CourseService course_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::ExamService exam_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::ClassroomService classroom_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::TermService term_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::GradeService grade_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::JudgeService judge_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::SpocService spoc_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::SigninService signin_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::YgdkService ygdk_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::EvaluationService evaluation_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::BykcService bykc_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::VenueReservationService venue_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::LibrarySeatService library_service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::FeatureService feature_service(http_client, cache, UBAANext::ConnectionMode::Direct);

    UBAANext::Model::FeatureRecord record;
    record.id = "sdk";
    UBAANext::CookieJar jar;
    auto vpn_url = UBAANext::VpnCipher::to_vpn_url("https://app.buaa.edu.cn/");
    auto encoded = UBAANext::base64_encode({'U', 'B', 'A', 'A'});

    REQUIRE_FALSE(auth_service.has_session());
    REQUIRE(record.id == "sdk");
    REQUIRE(jar.to_header("app.buaa.edu.cn").empty());
    REQUIRE_FALSE(vpn_url.empty());
    REQUIRE(encoded == "VUJBQQ==");
}
