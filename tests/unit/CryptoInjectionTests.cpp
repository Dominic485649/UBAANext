#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Service/VenueReservationService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace {

class RecordingCryptoProvider final : public UBAANext::ICryptoProvider {
public:
    UBAANext::Result<std::string> md5_hex(const std::string &input) override {
        last_md5_input = input;
        return std::string("injected-sign");
    }

    UBAANext::Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &) override {
        return std::vector<unsigned char>{};
    }

    UBAANext::Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &, const std::string &, const std::string &) override {
        return std::vector<unsigned char>{};
    }

    UBAANext::Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &, const std::string &) override {
        return std::vector<unsigned char>{};
    }

    UBAANext::Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &, const std::string &) override {
        return std::vector<unsigned char>{};
    }

    UBAANext::Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &, const std::string &) override {
        return std::string{};
    }

    std::string last_md5_input;
};

class VenueCryptoFixtureHttpClient final : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        last_headers = request.headers;
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin") {
            response.status_code = 302;
            response.headers["Location"] = "/venue-zhjs-server/sso/landing";
        } else if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/landing") {
            response.headers["Set-Cookie"] = "sso_buaa_zhjs_token=sso-token-1; Path=/; HttpOnly";
            response.body = R"JSON({"code":200,"data":{}})JSON";
        } else if (request.url == "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fcgyy.buaa.edu.cn%2Fvenue-zhjs-server%2Fsso%2FmanageLogin") {
            response.status_code = 302;
            response.headers["Location"] = "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin";
        } else if (request.url == "https://cgyy.buaa.edu.cn/venue-zhjs-server/api/login") {
            response.body = R"JSON({"code":200,"data":{"token":{"access_token":"access-token-1"}}})JSON";
        } else if (request.url.find("https://cgyy.buaa.edu.cn/venue-zhjs-server/api/orders/mine?") == 0) {
            response.body = R"JSON({"code":200,"data":{"content":[]}})JSON";
        } else {
            response.status_code = 500;
            response.body = R"JSON({"code":500,"message":"unexpected request"})JSON";
        }
        return response;
    }

    std::unordered_map<std::string, std::string> last_headers;
};

} // namespace

TEST_CASE("VenueReservationService 使用注入的 crypto provider 生成签名", "[service][crypto]") {
    VenueCryptoFixtureHttpClient http_client;
    UBAANext::MemoryCacheStore cache;
    RecordingCryptoProvider crypto;
    UBAANext::VenueReservationService service(http_client, cache, UBAANext::ConnectionMode::Direct, crypto);

    auto result = service.list_orders();

    REQUIRE(result);
    REQUIRE(http_client.last_headers.find("sign") != http_client.last_headers.end());
    CHECK(http_client.last_headers["sign"] == "injected-sign");
    CHECK_FALSE(crypto.last_md5_input.empty());
}
