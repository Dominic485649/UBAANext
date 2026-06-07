#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/CloudParser.hpp>
#include <UBAANext/Service/CloudService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace um = UBAANext;

namespace {

class CloudFixtureHttpClient : public um::IHttpClient {
public:
    um::Result<um::HttpResponse> send(const um::HttpRequest &request) override {
        requests.push_back(request);
        if (on_send) on_send(request, requests.size() - 1);
        if (network_error_next) {
            network_error_next = false;
            return um::make_error(um::ErrorCode::NetworkError, network_error_message);
        }
        if (next < responses.size()) return responses[next++];
        um::HttpResponse response;
        response.status_code = 200;
        response.body = R"JSON({"success":true,"data":[]})JSON";
        return response;
    }

    void push(int status, std::string body, std::unordered_map<std::string, std::string> headers = {}) {
        um::HttpResponse response;
        response.status_code = status;
        response.body = std::move(body);
        response.headers = std::move(headers);
        responses.push_back(std::move(response));
    }

    std::vector<um::HttpRequest> requests;
    std::vector<um::HttpResponse> responses;
    std::function<void(const um::HttpRequest &, std::size_t)> on_send;
    std::size_t next = 0;
    bool network_error_next = false;
    std::string network_error_message = "token=secret-token&Authorization: bearer-secret";
};

class CloudFixtureCookieStore : public um::ICookieStore {
public:
    um::Result<um::CookieJar> load() override {
        ++load_count;
        if (fail_load) return um::make_error(um::ErrorCode::StorageError, "token=secret-token&Authorization: bearer-secret");
        current_jar = loaded_jar;
        return loaded_jar;
    }

    um::Result<void> save(const um::CookieJar &cookies) override {
        current_jar = cookies;
        ++save_count;
        return {};
    }

    um::Result<void> save_current() override {
        ++save_count;
        return {};
    }

    um::Result<void> clear() override {
        current_jar.clear();
        ++clear_count;
        return {};
    }

    const um::CookieJar *current() const override { return expose_current ? &current_jar : nullptr; }

    um::CookieJar current_jar;
    um::CookieJar loaded_jar;
    int load_count = 0;
    int save_count = 0;
    int clear_count = 0;
    bool expose_current = true;
    bool fail_load = false;
};

class FakeCloudCryptoProvider : public um::ICryptoProvider {
public:
    um::Result<std::string> md5_hex(const std::string &input) override {
        (void)input;
        return std::string{"md5"};
    }

    um::Result<std::vector<unsigned char>> sha1_digest(const std::vector<unsigned char> &data) override {
        (void)data;
        return std::vector<unsigned char>{};
    }

    um::Result<std::vector<unsigned char>> aes_cbc_encrypt(const std::vector<unsigned char> &data, const std::string &key, const std::string &iv) override {
        (void)data;
        (void)key;
        (void)iv;
        return std::vector<unsigned char>{};
    }

    um::Result<std::vector<unsigned char>> aes_ecb_pkcs7_encrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        (void)data;
        (void)key;
        return std::vector<unsigned char>{};
    }

    um::Result<std::vector<unsigned char>> aes_ecb_pkcs7_decrypt(const std::vector<unsigned char> &data, const std::string &key) override {
        (void)data;
        (void)key;
        return std::vector<unsigned char>{};
    }

    um::Result<std::string> rsa_pkcs1_encrypt_base64(const std::vector<unsigned char> &data, const std::string &public_key_der_base64) override {
        rsa_plaintext.assign(data.begin(), data.end());
        rsa_public_key = public_key_der_base64;
        ++rsa_count;
        if (fail_rsa) return um::make_error(um::ErrorCode::CryptoError, "password=secret-pass&token=secret-token");
        return std::string{"encrypted-password"};
    }

    std::string rsa_plaintext;
    std::string rsa_public_key;
    int rsa_count = 0;
    bool fail_rsa = false;
};

class MemoryUploadSource : public um::IUploadSource {
public:
    MemoryUploadSource(std::string name, std::string data)
        : m_name(std::move(name)), m_data(std::move(data)) {}

    [[nodiscard]] std::string name() const override { return m_name; }
    [[nodiscard]] std::string content_type() const override { return "application/octet-stream"; }
    [[nodiscard]] um::Result<std::uint64_t> size() override { return static_cast<std::uint64_t>(m_data.size()); }
    [[nodiscard]] um::Result<void> rewind() override {
        m_offset = 0;
        return {};
    }
    [[nodiscard]] um::Result<std::size_t> read(unsigned char *buffer, std::size_t max_bytes) override {
        const auto remaining = m_data.size() - m_offset;
        const auto count = std::min(max_bytes, remaining);
        std::copy_n(reinterpret_cast<const unsigned char *>(m_data.data() + m_offset), count, buffer);
        m_offset += count;
        return count;
    }

private:
    std::string m_name;
    std::string m_data;
    std::size_t m_offset = 0;
};

} // namespace

TEST_CASE("CloudParser 解析 reference 根目录、目录、容量和分享 envelope", "[CloudParser]") {
    auto roots = um::Parser::parse_cloud_roots(nlohmann::json::parse(R"JSON({"data":[{"docid":"root-1","doc_lib_name":"个人文档库","doc_lib_type":"user_doc_lib","totalsize":-1}]})JSON"));
    REQUIRE(roots.size() == 1);
    CHECK(roots[0].id == "root-1");
    CHECK(roots[0].name == "个人文档库");
    CHECK(roots[0].type == "user_doc_lib");
    CHECK(roots[0].is_dir());

    auto dir = um::Parser::parse_cloud_dir(nlohmann::json::parse(R"JSON({"data":{"dirs":[{"id":"dir-1","name":"资料","size":"-1"}],"files":[{"gnsId":"file-1","title":"讲义.pdf","size":2048,"parentId":"dir-1"}]}})JSON"));
    REQUIRE(dir.dirs.size() == 1);
    REQUIRE(dir.files.size() == 1);
    CHECK(dir.dirs[0].type == "dir");
    CHECK(dir.files[0].id == "file-1");
    CHECK(dir.files[0].size == "2048");

    auto size = um::Parser::parse_cloud_size(nlohmann::json::parse(R"JSON({"data":{"totalsize":"2048","filenum":1,"dirnum":2}})JSON"));
    CHECK(size.bytes == "2048");
    CHECK(size.file_count == "1");
    CHECK(size.dir_count == "2");

    auto shares = um::Parser::parse_cloud_shares(nlohmann::json::parse(R"JSON({"data":[{"id":"share-1","title":"分享资料","item":{"docid":"file-1","permission":["read","download"]},"expiration":"never"}]})JSON"));
    REQUIRE(shares.size() == 1);
    CHECK(shares[0].id == "share-1");
    CHECK(shares[0].item_id == "file-1");
    CHECK(shares[0].permissions == "read,download");
    CHECK(shares[0].url.find("https://bhpan.buaa.edu.cn/link/share-1") == 0);
}

TEST_CASE("CloudService 从 current cookie 读取 token 并设置 Authorization", "[service][cloud]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "current-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 1);
    CHECK(cookies.load_count == 0);
    CHECK(http.requests[0].headers.at("Authorization") == "Bearer current-token");
    CHECK(http.requests[0].url.find("https://bhpan.buaa.edu.cn/api/efast/v1/entry-doc-lib") == 0);
    REQUIRE(result->size() == 1);
    CHECK((*result)[0].id == "root-1");
}

TEST_CASE("CloudService current 缺 token 时从 load cookie 读取", "[service][cloud]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    cookies.loaded_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "loaded-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::User);

    REQUIRE(result);
    REQUIRE(cookies.load_count == 1);
    REQUIRE(http.requests.size() == 1);
    CHECK(http.requests[0].headers.at("Authorization") == "Bearer loaded-token");
    CHECK(http.requests[0].url.find("type=user_doc_lib") != std::string::npos);
}

TEST_CASE("CloudService 无 CookieStore 时稳定失败且不发请求", "[service][cloud]") {
    CloudFixtureHttpClient http;
    um::MemoryCacheStore cache;
    um::CloudService service(http, nullptr, cache, um::ConnectionMode::Direct);

    auto result = service.user_root_record();

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::UnsupportedCookiePersistence);
    CHECK(http.requests.empty());
}

TEST_CASE("CloudService list 和 size 设置分享 token 请求头", "[service][cloud]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[{"docid":"file-1","name":"a.txt","size":1}]}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"totalsize":"1","filenum":1,"dirnum":0}})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "token-value");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    um::CloudListQuery query;
    query.doc_id = "dir-1";
    query.token = "share-token";
    auto list = service.list_records(query);
    auto size = service.size_record(query);

    REQUIRE(list);
    REQUIRE(size);
    REQUIRE(http.requests.size() == 2);
    CHECK(http.requests[0].headers.at("x-as-authorization") == "Bearer share-token");
    CHECK(http.requests[0].body.find("\"docid\":\"dir-1\"") != std::string::npos);
    CHECK(http.requests[1].headers.at("x-as-authorization") == "Bearer share-token");
    CHECK((*list)[0].id == "file-1");
    CHECK(size->fields.at("bytes") == "1");
}

TEST_CASE("CloudService 401 后强制刷新 token 并重试", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(401, R"JSON({"success":false,"message":"expired"})JSON");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=lc"}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "old-token");
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "new-token");
        }
    };
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 5);
    CHECK(http.requests[0].headers.at("Authorization") == "Bearer old-token");
    CHECK(http.requests[4].headers.at("Authorization") == "Bearer new-token");
    CHECK(cookies.load_count == 0);
}

TEST_CASE("CloudService 登录重定向仅用请求头携带 login_challenge", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "loaded-token");
        }
    };
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 4);
    REQUIRE(http.requests[1].headers.count("Cookie") == 1);
    CHECK(http.requests[1].headers.at("Cookie").find("login_challenge=challenge-token") != std::string::npos);
    CHECK(cookies.current_jar.get_cookie("bhpan.buaa.edu.cn", "login_challenge") == std::nullopt);
}

TEST_CASE("CloudService 从 SSO signin HTML 提取 callback 后再刷新 token", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html></html>)HTML");
    http.push(200, R"HTML(<html><script>window.location.href='\/anyshare\/oauth2\/login\/callback?login_challenge=challenge-token&amp;state=safe';</script></html>)HTML");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"oauth2_token":"json-token"})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 6);
    CHECK(http.requests[3].url.find("/anyshare/oauth2/login/callback") != std::string::npos);
    REQUIRE(http.requests[3].headers.count("Cookie") == 1);
    CHECK(http.requests[3].headers.at("Cookie").find("login_challenge=challenge-token") != std::string::npos);
    CHECK(http.requests[5].headers.at("Authorization") == "Bearer json-token");
}

TEST_CASE("CloudService OAuth2 signin 表单按 HTML 成功控件语义提交", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html><form action='/oauth2/signin'><input name='csrf_token' value='csrf'><input type='checkbox' name='unchecked_box' value='bad'><input type='checkbox' name='checked_box' checked><input disabled name='disabled_field' value='bad'><button disabled name='disabled_submit' value='bad'>Bad</button><button type='submit' name='good_submit' value='yes'>OK</button></form></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "form-token");
        }
    };
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 8);
    const auto &submit_url = http.requests[4].url;
    CHECK(submit_url.find("/oauth2/signin") != std::string::npos);
    CHECK(submit_url.find("csrf_token=csrf") != std::string::npos);
    CHECK(submit_url.find("checked_box=on") != std::string::npos);
    CHECK(submit_url.find("good_submit=yes") != std::string::npos);
    CHECK(submit_url.find("login_challenge") == std::string::npos);
    CHECK(submit_url.find("unchecked_box=bad") == std::string::npos);
    CHECK(submit_url.find("disabled_field") == std::string::npos);
    CHECK(submit_url.find("disabled_submit") == std::string::npos);
    CHECK(http.requests[7].headers.at("Authorization") == "Bearer form-token");
}

TEST_CASE("CloudService OAuth2 signin 表单可用保存凭据填充登录字段", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html><form action='/oauth2/signin'><input name='account' value=''><input type='password' name='credential'><input type='hidden' name='csrf_token' value='csrf'><button type='submit'>OK</button></form></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "credential-token");
        }
    };
    um::CloudLoginCredentials credentials{"student-user", "secret-pass"};
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct, credentials);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 8);
    CHECK(http.requests[4].method == um::HttpMethod::Post);
    CHECK(http.requests[4].url.find("account=student-user") == std::string::npos);
    CHECK(http.requests[4].url.find("credential=secret-pass") == std::string::npos);
    CHECK(http.requests[4].body.find("account=student-user") != std::string::npos);
    CHECK(http.requests[4].body.find("credential=secret-pass") != std::string::npos);
    CHECK(http.requests[4].body.find("csrf_token=csrf") != std::string::npos);
    CHECK(http.requests[4].headers.at("Origin") == "https://bhpan.buaa.edu.cn");
    CHECK(http.requests[7].headers.at("Authorization") == "Bearer credential-token");
}

TEST_CASE("CloudService OAuth2 signin Next 页面按 AnyShare JSON+RSA 协议提交", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html><script id="__NEXT_DATA__" type="application/json">{"props":{"pageProps":{"csrftoken":"csrf-token","challenge":"page-challenge","device":{"client_type":"web","description":"browser","name":"Edge","udids":"device-id"}}}}</script><form action='/oauth2/signin'><input name='account'><input type='password' name='password'><button type='button'>登录</button></form></html>)HTML");
    http.push(200, R"JSON({"redirect":"https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"})JSON");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    FakeCloudCryptoProvider crypto;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "json-token");
        }
    };
    um::CloudLoginCredentials credentials{"student-user", "secret-pass"};
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct, crypto, credentials);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 8);
    REQUIRE(crypto.rsa_count == 1);
    CHECK(crypto.rsa_plaintext == "secret-pass");
    CHECK(crypto.rsa_public_key.find("MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A") == 0);
    const auto &signin = http.requests[4];
    CHECK(signin.method == um::HttpMethod::Post);
    CHECK(signin.url.find("/oauth2/signin") != std::string::npos);
    CHECK(signin.url.find("secret-pass") == std::string::npos);
    REQUIRE(signin.headers.count("Content-Type") == 1);
    CHECK(signin.headers.at("Content-Type") == "application/json");
    CHECK(signin.headers.at("Origin") == "https://bhpan.buaa.edu.cn");
    CHECK(signin.body.find("secret-pass") == std::string::npos);
    auto body = nlohmann::json::parse(signin.body);
    CHECK(body["_csrf"] == "csrf-token");
    CHECK(body["challenge"] == "page-challenge");
    CHECK(body["account"] == "student-user");
    CHECK(body["password"] == "encrypted-password");
    CHECK(body["remember"] == false);
    CHECK(body["device"]["client_type"] == "web");
    CHECK(body["vcode"]["id"] == "");
    CHECK(body["dualfactorauthinfo"]["OTP"]["OTP"] == "");
    CHECK(http.requests[5].url.find("/anyshare/oauth2/login/callback") != std::string::npos);
    CHECK(http.requests[7].headers.at("Authorization") == "Bearer json-token");
}

TEST_CASE("CloudService OAuth2 signin JSON 后继续跟随 callback 脚本跳转", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html></html>)HTML");
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/oauth2/signin?login_challenge=challenge-token"}});
    http.push(200, R"HTML(<html><script id="__NEXT_DATA__" type="application/json">{"props":{"pageProps":{"csrftoken":"csrf-token","challenge":"page-challenge","device":{"client_type":"web"}}}}</script><form action='/oauth2/signin'><input name='account'><input type='password' name='password'><button type='button'>登录</button></form></html>)HTML");
    http.push(200, R"JSON({"data":{"redirect":"https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"}})JSON");
    http.push(200, R"HTML(<html><script>location.href='/anyshare/zh-cn/portal';</script></html>)HTML");
    http.push(200, R"HTML(<html><body>portal</body></html>)HTML");
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    FakeCloudCryptoProvider crypto;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "script-token");
        }
    };
    um::CloudLoginCredentials credentials{"student-user", "secret-pass"};
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct, crypto, credentials);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 9);
    CHECK(http.requests[5].url.find("/anyshare/oauth2/login/callback") != std::string::npos);
    CHECK(http.requests[6].url.find("/anyshare/zh-cn/portal") != std::string::npos);
    CHECK(http.requests[8].headers.at("Authorization") == "Bearer script-token");
}

TEST_CASE("CloudService refreshToken 首次无 token 时按前端 isforced=false 重试", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    http.push(302, "", {{"Location", "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token"}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"code":200,"message":"ok"})JSON");
    http.push(200, R"JSON({"code":200,"message":"ok"})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken?isforced=false") != std::string::npos) {
            cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "forced-refresh-token");
        }
    };
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 5);
    CHECK(http.requests[2].url == "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/refreshToken");
    CHECK(http.requests[3].url == "https://bhpan.buaa.edu.cn/anyshare/oauth2/login/refreshToken?isforced=false");
    CHECK(http.requests[3].headers.at("Accept") == "application/json, text/plain, */*");
    CHECK(http.requests[3].headers.at("X-Requested-With") == "XMLHttpRequest");
    CHECK(http.requests[3].headers.at("Cache-Control") == "no-cache");
    CHECK(http.requests[4].headers.at("Authorization") == "Bearer forced-refresh-token");
}

TEST_CASE("CloudService WebVPN 不复用 direct 域 OAuth2 token", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    const auto callback_url = um::VpnCipher::to_vpn_url("https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token");
    const auto gateway = std::string{"https://d.buaa.edu.cn"};
    REQUIRE(callback_url.rfind(gateway, 0) == 0);
    http.push(302, "", {{"Location", callback_url.substr(gateway.size())}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "direct-token");
    cookies.loaded_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "loaded-direct-token");
    um::MemoryCacheStore cache;
    http.on_send = [&](const um::HttpRequest &request, std::size_t) {
        if (request.url.find("refreshToken") != std::string::npos) {
            cookies.current_jar.set_cookie("d.buaa.edu.cn", "client.oauth2_token", "vpn-token");
        }
    };
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::WebVPN);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 4);
    CHECK(http.requests[0].url.find("https://d.buaa.edu.cn/https/") == 0);
    CHECK(http.requests[0].url.find("/anyshare/oauth2/login") != std::string::npos);
    CHECK(http.requests[3].headers.at("Authorization") == "Bearer vpn-token");
}

TEST_CASE("CloudService WebVPN 相对重定向按 VPN 网关还原逻辑目标", "[service][cloud][session]") {
    CloudFixtureHttpClient http;
    const auto callback_url = um::VpnCipher::to_vpn_url("https://bhpan.buaa.edu.cn/anyshare/oauth2/login/callback?login_challenge=challenge-token");
    const auto gateway = std::string{"https://d.buaa.edu.cn"};
    REQUIRE(callback_url.rfind(gateway, 0) == 0);
    http.push(302, "", {{"Location", callback_url.substr(gateway.size())}});
    http.push(200, R"JSON({"success":true})JSON");
    http.push(200, R"JSON({"success":true,"oauth2_token":"json-token"})JSON");
    http.push(200, R"JSON({"success":true,"data":[{"docid":"root-1","doc_lib_name":"个人文档库"}]})JSON");
    CloudFixtureCookieStore cookies;
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::WebVPN);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 4);
    CHECK(http.requests[1].url.find("/anyshare/oauth2/login/callback") != std::string::npos);
    REQUIRE(http.requests[1].headers.count("Cookie") == 1);
    CHECK(http.requests[1].headers.at("Cookie").find("login_challenge=challenge-token") != std::string::npos);
    CHECK(http.requests[3].headers.at("Authorization") == "Bearer json-token");
}

TEST_CASE("CloudService 写操作未确认时 fail closed 且不发请求", "[service][cloud][write]") {
    CloudFixtureHttpClient http;
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.create_dir("parent-1", "New Folder");

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::InvalidArgument);
    CHECK(http.requests.empty());
}

TEST_CASE("CloudService copy 使用分享 token 作为 x-as-authorization", "[service][cloud][write]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"docid":"copy-1"}})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    um::WriteOperationGate gate;
    gate.confirmed = true;
    gate.allow_write_operations = true;
    gate.operation = "file copy";
    service.set_write_operation_gate(gate);

    um::CloudItemRef item;
    item.doc_id = "share-file-1";
    item.token = "share-token";
    auto result = service.copy_item(item, "parent-2");

    REQUIRE(result);
    REQUIRE(http.requests.size() == 1);
    CHECK(http.requests[0].headers.at("Authorization") == "Bearer cloud-token");
    CHECK(http.requests[0].headers.at("x-as-authorization") == "Bearer share-token");
    auto body = nlohmann::json::parse(http.requests[0].body);
    CHECK(body["docid"] == "share-file-1");
    CHECK(body["destparent"] == "parent-2");
}

TEST_CASE("CloudService 写操作接受 2xx 空响应", "[service][cloud][write]") {
    CloudFixtureHttpClient http;
    http.push(200, "");
    http.push(200, "");
    http.push(200, "");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    um::WriteOperationGate gate;
    gate.confirmed = true;
    gate.allow_write_operations = true;
    gate.operation = "file write";
    service.set_write_operation_gate(gate);

    auto renamed = service.rename_item("item-1", "renamed");
    auto deleted = service.delete_item("item-1");
    auto share_deleted = service.share_delete("share-1");

    REQUIRE(renamed);
    REQUIRE(deleted);
    REQUIRE(share_deleted);
    CHECK(renamed->summary.status == "renamed");
    CHECK(deleted->summary.status == "deleted");
    CHECK(share_deleted->summary.status == "share-deleted");
}

TEST_CASE("CloudService 小文件上传执行 preupload begin PUT endupload", "[service][cloud][upload]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-1","rev":"rev-1"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-1"}})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    um::WriteOperationGate gate;
    gate.confirmed = true;
    gate.allow_write_operations = true;
    gate.operation = "file upload";
    service.set_write_operation_gate(gate);
    MemoryUploadSource source("hello.txt", "hello");

    um::CloudUploadRequest request;
    request.parent_id = "parent-1";
    auto result = service.upload_file(request, source);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 4);
    CHECK(http.requests[0].url.find("/file/predupload") != std::string::npos);
    CHECK(http.requests[1].url.find("/file/osbeginupload") != std::string::npos);
    CHECK(http.requests[2].method == um::HttpMethod::Put);
    CHECK(http.requests[2].url == "https://upload.example/small");
    CHECK(http.requests[2].body == "hello");
    CHECK(http.requests[3].url.find("/file/osendupload") != std::string::npos);
}

TEST_CASE("CloudService 大文件上传按 20MiB 分片读取 ETag", "[service][cloud][upload]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"docid":"big-1","rev":"rev-1","uploadid":"upload-1"}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequests":{"1":["PUT","https://upload.example/part1"],"2":["PUT","https://upload.example/part2"]}}})JSON");
    http.push(200, "", {{"ETag", "\"etag-1\""}});
    http.push(200, "", {{"ETag", "\"etag-2\""}});
    http.push(200, "<CompleteMultipartUploadResult/>\r\n--boundary\r\n{\"authrequest\":[\"POST\",\"https://upload.example/complete\"]}\r\n--boundary--");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"big-1"}})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    um::WriteOperationGate gate;
    gate.confirmed = true;
    gate.allow_write_operations = true;
    gate.operation = "file upload";
    service.set_write_operation_gate(gate);
    std::string data(20 * 1024 * 1024 + 1, 'x');
    MemoryUploadSource source("big.bin", data);
    um::CloudUploadRequest request;
    request.parent_id = "parent-1";

    auto result = service.upload_file(request, source);

    REQUIRE(result);
    REQUIRE(http.requests.size() == 8);
    CHECK(http.requests[3].body.size() == 20 * 1024 * 1024);
    CHECK(http.requests[4].body.size() == 1);
    auto complete_body = nlohmann::json::parse(http.requests[5].body);
    CHECK(complete_body["partinfo"]["1"][0] == "etag-1");
    CHECK(complete_body["partinfo"]["2"][0] == "etag-2");
    CHECK(http.requests[6].method == um::HttpMethod::Post);
    CHECK(http.requests[6].url == "https://upload.example/complete");
}

TEST_CASE("CloudService 业务错误和 Cookie 读取错误会脱敏", "[service][cloud][security]") {
    CloudFixtureHttpClient http;
    http.push(200, R"JSON({"success":false,"message":"token=secret-token&Authorization: bearer-secret"})JSON");
    CloudFixtureCookieStore cookies;
    cookies.current_jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "token-value");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    auto result = service.root_records(um::CloudRootKind::All);

    REQUIRE_FALSE(result);
    CHECK(result.error().code == um::ErrorCode::NetworkError);
    CHECK(result.error().message.find("secret-token") == std::string::npos);
    CHECK(result.error().message.find("bearer-secret") == std::string::npos);
    CHECK(result.error().message.find("[REDACTED]") != std::string::npos);

    CloudFixtureHttpClient no_http;
    CloudFixtureCookieStore failing_cookies;
    failing_cookies.fail_load = true;
    um::CloudService failing_service(no_http, &failing_cookies, cache, um::ConnectionMode::Direct);
    auto failing = failing_service.root_records(um::CloudRootKind::All);

    REQUIRE_FALSE(failing);
    CHECK(failing.error().code == um::ErrorCode::StorageError);
    CHECK(failing.error().message.find("secret-token") == std::string::npos);
    CHECK(failing.error().message.find("bearer-secret") == std::string::npos);
    CHECK(failing.error().message.find("[REDACTED]") != std::string::npos);
}
