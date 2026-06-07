#include <UBAANext/Crypto/ProtocolCrypto.hpp>
#include <UBAANext/Service/WifiService.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

class TestNetworkEnvironment final : public UBAANext::INetworkEnvironment {
public:
    UBAANext::Result<bool> is_on_campus_network() override {
        return campus;
    }
    UBAANext::Result<std::string> local_ipv4() override {
        if (ip.empty()) return UBAANext::make_error(UBAANext::ErrorCode::NetworkError, "No local IP");
        return ip;
    }
    bool campus = true;
    std::string ip = "10.0.0.2";
};

class TestCryptoProvider final : public UBAANext::ICryptoProvider {
public:
    UBAANext::Result<std::string> md5_hex(const std::string &input) override {
        (void)input;
        return "md5";
    }
    UBAANext::Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) override {
        last_sha1_input.assign(data.begin(), data.end());
        return std::vector<unsigned char>{0x01, 0x02, 0x03};
    }
    UBAANext::Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &, const std::string &, const std::string &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedCrypto, "unsupported");
    }
    UBAANext::Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &, const std::string &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedCrypto, "unsupported");
    }
    UBAANext::Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &, const std::string &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedCrypto, "unsupported");
    }
    UBAANext::Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &, const std::string &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedCrypto, "unsupported");
    }
    std::string last_sha1_input;
};

class WifiHttpClient final : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        requests.push_back(request);
        UBAANext::HttpResponse response;
        response.status_code = 200;
        if (request.url == "http://gw.buaa.edu.cn") {
            response.body = "<html><meta http-equiv='refresh' content='0; url=index_1.html?ac_id=5&foo=bar'></html>";
        } else if (request.url.find("https://gw.buaa.edu.cn/cgi-bin/get_challenge?") == 0) {
            response.body = R"({"challenge":"challenge-token"})";
        } else if (request.url.find("https://gw.buaa.edu.cn/cgi-bin/srun_portal?") == 0) {
            response.body = R"({"error":"ok"})";
        } else {
            response.body = R"({"error":"unexpected"})";
        }
        return response;
    }
    std::vector<UBAANext::HttpRequest> requests;
};

} // namespace

TEST_CASE("ProtocolCrypto 对齐 buaa-api HMAC-MD5 和 xencode golden", "[crypto][wifi]") {
    const auto hmac = UBAANext::Crypto::hmac_md5_digest({'K', 'e', 'y'}, {'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd'});
    CHECK(UBAANext::Crypto::bytes_to_hex(hmac) == "219e14bef981f117479a7695dacb10c7");

    const std::string key = "8e4e83f094924913acc6a9d5149015aafc898bd38ba8f45be6bd0f9edd450403";
    const auto encoded = UBAANext::Crypto::srun_xencode({'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd'},
                                                        std::vector<unsigned char>(key.begin(), key.end()));
    CHECK(encoded == "{SRBX1}9GAfJJT7wdSzFKeNohuv6+==");
}

TEST_CASE("WifiService 未确认时 fail closed 且不探测网络", "[service][wifi][write-gate]") {
    WifiHttpClient http;
    TestNetworkEnvironment env;
    TestCryptoProvider crypto;
    UBAANext::WifiService service(http, &env, crypto, {"user", "pass"});

    auto result = service.login();

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(http.requests.empty());
}

TEST_CASE("WifiService 校园网环境失败时不请求网关", "[service][wifi]") {
    WifiHttpClient http;
    TestNetworkEnvironment env;
    env.campus = false;
    TestCryptoProvider crypto;
    UBAANext::WifiService service(http, &env, crypto, {"user", "pass"});
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "wifi login"));

    auto result = service.login();

    REQUIRE_FALSE(result);
    CHECK(result.error().code == UBAANext::ErrorCode::NetworkError);
    CHECK(http.requests.empty());
}

TEST_CASE("WifiService 登录按 reference 计算并调用 portal", "[service][wifi]") {
    WifiHttpClient http;
    TestNetworkEnvironment env;
    TestCryptoProvider crypto;
    UBAANext::WifiService service(http, &env, crypto, {"20260000", "secret"});
    UBAANext::PlatformCapabilities capabilities;
    capabilities.write_operations = true;
    service.set_write_operation_gate(UBAANext::confirmed_write_operation(capabilities, "wifi login"));

    auto result = service.login();

    REQUIRE(result);
    CHECK(result->accepted);
    REQUIRE(http.requests.size() == 3);
    const auto &portal = http.requests.back().url;
    CHECK(portal.find("action=login") != std::string::npos);
    CHECK(portal.find("username=20260000") != std::string::npos);
    CHECK(portal.find("password=%7BMD5%7D") != std::string::npos);
    CHECK(portal.find("ac_id=5") != std::string::npos);
    CHECK(portal.find("ip=10.0.0.2") != std::string::npos);
    CHECK(portal.find("chksum=010203") != std::string::npos);
    CHECK(portal.find("info=%7BSRBX1%7D") != std::string::npos);
    CHECK(crypto.last_sha1_input.find("challenge-token20260000") != std::string::npos);
}
