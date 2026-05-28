#include <UBAANext/Service/YgdkService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr const char *kOauthUrl = "https://app.buaa.edu.cn/uc/api/oauth/index?redirect=https%3A%2F%2Fygdk.buaa.edu.cn%2F%23%2Fhome&appid=200230221144501510&state=STATE&qrcode=1";
constexpr const char *kLoginUrl = "https://ygdk.buaa.edu.cn/api/Front/Clockin/User/campusAppLogin?code=test-code";
constexpr const char *kUploadUrl = "https://ygdk.buaa.edu.cn/api/Front/Upload/File/post";
constexpr const char *kClassifyUrl = "https://ygdk.buaa.edu.cn/api/Front/Clockin/Classify/getList";
constexpr const char *kItemsUrl = "https://ygdk.buaa.edu.cn/api/Front/Clockin/Item/getList?classify_id=c2&limit=1000&page=1";
constexpr const char *kSubmitUrl = "https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/clockin";
constexpr const char *kRecordsUrl = "https://ygdk.buaa.edu.cn/api/Front/Clockin/Clockin/getList?classify_id=c2&limit=20&page=1&user_id=u-1";

class YgdkFixtureHttpClient : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        requested_urls.push_back(request.url);
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == kOauthUrl) {
            response.status_code = 302;
            response.headers["Location"] = "https://ygdk.buaa.edu.cn/#/home?code=test-code";
        } else if (request.url == kLoginUrl) {
            response.body = R"JSON({"code":1,"result":{"uid":"u-1","token":"token-1"}})JSON";
        } else if (request.url == kClassifyUrl) {
            response.body = R"JSON({"code":1,"result":{"list":[{"classify_id":"c1","name":"劳动教育"},{"classify_id":"c2","name":"体育锻炼"}]}})JSON";
        } else if (request.url == kItemsUrl) {
            response.body = R"JSON({"code":1,"result":{"list":[{"item_id":"walk-1","name":"健走","sort":"1"},{"item_id":"run-1","name":"跑步","sort":"2"}]}})JSON";
        } else if (request.url == kUploadUrl) {
            ++upload_requests;
            last_upload_body = request.body;
            last_upload_content_type = request.headers.at("Content-Type");
            response.body = R"JSON({"code":1,"result":{"file_name":"uploaded.png"}})JSON";
        } else if (request.url == kSubmitUrl) {
            ++submit_requests;
            last_submit_body = request.body;
            response.body = R"JSON({"code":1,"result":{"record_id":"record-1"}})JSON";
        } else if (request.url == kRecordsUrl) {
            response.body = R"JSON({"code":1,"result":{"list":[{"record_id":"record-1","item_name":"跑步","state":"approved","place":"操场","start_time":"2026-05-16 20:00","end_time":"2026-05-16 21:00","create_time_fmt":"2026-05-16 21:05"}]}})JSON";
        } else {
            response.status_code = 404;
            response.body = R"JSON({"code":0,"msg":"unexpected url"})JSON";
        }
        return response;
    }

    int upload_requests = 0;
    int submit_requests = 0;
    std::string last_upload_body;
    std::string last_upload_content_type;
    std::string last_submit_body;
    std::vector<std::string> requested_urls;
};

std::string url_decode(std::string value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            auto hex = value.substr(i + 1, 2);
            char *end = nullptr;
            auto decoded = std::strtol(hex.c_str(), &end, 16);
            if (end && *end == '\0') {
                out.push_back(static_cast<char>(decoded));
                i += 2;
                continue;
            }
        }
        out.push_back(value[i] == '+' ? ' ' : value[i]);
    }
    return out;
}

std::map<std::string, std::string> parse_form(const std::string &body) {
    std::map<std::string, std::string> form;
    size_t pos = 0;
    while (pos <= body.size()) {
        auto amp = body.find('&', pos);
        auto part = body.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
        auto eq = part.find('=');
        if (eq != std::string::npos) form[url_decode(part.substr(0, eq))] = url_decode(part.substr(eq + 1));
        if (amp == std::string::npos) break;
        pos = amp + 1;
    }
    return form;
}

UBAANext::UploadPart test_photo() {
    return UBAANext::UploadPart{"file", "ubaanext-ygdk-test.png", "image/png", {'p', 'n', 'g'}};
}

} // namespace

TEST_CASE("YgdkService 默认提交参数对齐 UBAA", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto photo = test_photo();
    auto result = service.submit_clockin("", "2026-05-16 20:00", "2026-05-16 21:00", "", false, photo);

    REQUIRE(result);
    CHECK(http_client.upload_requests == 1);
    CHECK(http_client.submit_requests == 1);
    CHECK(http_client.last_upload_content_type.find("multipart/form-data") != std::string::npos);
    CHECK(http_client.last_upload_body.find("filename=\"ubaanext-ygdk-test.png\"") != std::string::npos);
    CHECK(http_client.last_upload_body.find("Content-Type: image/png") != std::string::npos);

    auto form = parse_form(http_client.last_submit_body);
    CHECK(form["start_time"] == "1778932800");
    CHECK(form["end_time"] == "1778936400");
    CHECK(form["place"] == "操场");
    CHECK(form["isopen"] == "0");
    CHECK(form["images"] == R"(["uploaded.png"])");
    CHECK(form["classify_id"] == "c2");
    CHECK(form["item_id"] == "run-1");
    CHECK(form["item_name"] == "跑步");
    CHECK(form["form_time_fmt"] == "2026-05-16 20:00-21:00");
    CHECK(result->summary.fields.at("itemId") == "run-1");
    CHECK(result->summary.fields.at("place") == "操场");
    CHECK(result->summary.fields.at("image") == "uploaded.png");
}

TEST_CASE("YgdkService 接受 ISO 时间输入", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto photo = test_photo();
    auto result = service.submit_clockin("", "2026-05-16T20:00:00", "2026-05-16T21:00:00", "", false, photo);

    REQUIRE(result);
    auto form = parse_form(http_client.last_submit_body);
    CHECK(form["start_time"] == "1778932800");
    CHECK(form["end_time"] == "1778936400");
    CHECK(form["form_time_fmt"] == "2026-05-16 20:00-21:00");
}

TEST_CASE("YgdkService 拒绝带时区 ISO 时间", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto result = service.submit_clockin("", "2026-05-16T20:00:00Z", "2026-05-16T21:00:00Z", "", false, test_photo());

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.requested_urls.empty());
}

TEST_CASE("YgdkService 拒绝只提供单侧时间且不发起网络请求", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto result = service.submit_clockin("", "2026-05-16 20:00", "", "", false, test_photo());

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.requested_urls.empty());
}

TEST_CASE("YgdkService 空时间默认生成一小时时段", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto result = service.submit_clockin("", "", "", "", false, UBAANext::UploadPart{});

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.requested_urls.empty());
}

TEST_CASE("YgdkService 历史记录使用体育分类", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto records = service.record_list();

    REQUIRE(records);
    REQUIRE(records->size() == 1);
    CHECK((*records)[0].id == "record-1");
    CHECK((*records)[0].item_name == "跑步");
}

TEST_CASE("YgdkService 历史记录业务错误消息会脱敏", "[service][ygdk][security]") {
    class SensitiveRecordsErrorHttpClient : public YgdkFixtureHttpClient {
    public:
        UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
            if (request.url == kRecordsUrl) {
                requested_urls.push_back(request.url);
                UBAANext::HttpResponse response;
                response.status_code = 200;
                response.body = R"JSON({"code":0,"msg":"token=token-secret&Authorization: bearer-secret&photo_path=C:/secret/ygdk-photo.jpg"})JSON";
                return response;
            }
            return YgdkFixtureHttpClient::send(request);
        }
    } http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);

    auto records = service.records();

    REQUIRE_FALSE(records);
    CHECK(records.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(records.error().message.find("token-secret") == std::string::npos);
    CHECK(records.error().message.find("bearer-secret") == std::string::npos);
    CHECK(records.error().message.find("C:/secret/ygdk-photo.jpg") == std::string::npos);
    CHECK(records.error().message.find("[REDACTED]") != std::string::npos);
    CHECK(http_client.upload_requests == 0);
    CHECK(http_client.submit_requests == 0);
}

TEST_CASE("YgdkService 无效项目不会上传默认图片", "[service][ygdk]") {
    YgdkFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto result = service.submit_clockin("missing-item", "2026-05-16 20:00", "2026-05-16 21:00", "", false, test_photo());

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http_client.upload_requests == 0);
    CHECK(http_client.submit_requests == 0);
}

TEST_CASE("YgdkService 无跑步项目时选择 sort 最小项目", "[service][ygdk]") {
    class FallbackItemHttpClient : public YgdkFixtureHttpClient {
    public:
        UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
            if (request.url == kItemsUrl) {
                requested_urls.push_back(request.url);
                UBAANext::HttpResponse response;
                response.status_code = 200;
                response.body = R"JSON({"code":1,"result":{"list":[{"item_id":"basketball-1","name":"篮球","sort":"5"},{"item_id":"walk-1","name":"健走","sort":"1"}]}})JSON";
                return response;
            }
            return YgdkFixtureHttpClient::send(request);
        }
    } http_client;
    UBAANext::MemoryCacheStore cache;
    UBAANext::YgdkService service(http_client, cache, UBAANext::ConnectionMode::Direct);
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "ygdk submit"));

    auto photo = test_photo();
    auto result = service.submit_clockin("", "2026-05-16 20:00", "2026-05-16 21:00", "操场", false, photo);

    REQUIRE(result);
    auto form = parse_form(http_client.last_submit_body);
    CHECK(form["item_id"] == "walk-1");
    CHECK(form["item_name"] == "健走");
}
