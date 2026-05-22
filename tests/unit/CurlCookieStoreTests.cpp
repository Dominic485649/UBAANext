#include <UBAANext/Platform/Curl/CurlCookieStore.hpp>
#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>

#include <UBAANextMocks/MockSecureStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;
namespace curl = UBAANext::Platform::Curl;

TEST_CASE("CurlCookieStore 无安全存储时显式不支持持久化", "[curl][cookie]") {
    um::CookieJar live;
    curl::CurlCookieStore store(live);

    auto loaded = store.load();
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().code == um::ErrorCode::UnsupportedCookiePersistence);

    um::CookieJar cookies;
    cookies.set_cookie("sso.buaa.edu.cn", "SESSION", "abc");
    auto saved = store.save(cookies);
    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().code == um::ErrorCode::UnsupportedCookiePersistence);
    CHECK(live.to_header("sso.buaa.edu.cn").empty());

    live.set_cookie("sso.buaa.edu.cn", "SESSION", "abc");
    auto current_saved = store.save_current();
    REQUIRE_FALSE(current_saved.has_value());
    CHECK(current_saved.error().code == um::ErrorCode::UnsupportedCookiePersistence);
    CHECK(live.to_header("sso.buaa.edu.cn") == "SESSION=abc");

    auto cleared = store.clear();
    REQUIRE_FALSE(cleared.has_value());
    CHECK(cleared.error().code == um::ErrorCode::UnsupportedCookiePersistence);
}

TEST_CASE("CurlCookieStore 通过 SecureStore 保存版本化 Cookie blob", "[curl][cookie]") {
    um::CookieJar live;
    UBAANextMocks::MockSecureStore secure_store;
    curl::CurlCookieStore store(live, secure_store);

    um::CookieJar cookies;
    cookies.set_cookie("sso.buaa.edu.cn", "/", "SESSION", "abc");
    cookies.set_cookie("byxt.buaa.edu.cn", "/app", "TOKEN", "xyz");

    auto saved = store.save(cookies);
    REQUIRE(saved.has_value());

    auto blob = secure_store.get_string(curl::CurlCookieStore::default_key());
    REQUIRE(blob.has_value());
    CHECK(blob->find("UBAANext-Cookies-v1\n") == 0);
    CHECK(blob->find("sso.buaa.edu.cn\t/\tSESSION\tabc") != std::string::npos);
    CHECK(live.to_header("byxt.buaa.edu.cn") == "TOKEN=xyz");

    live.clear();
    auto loaded = store.load();
    REQUIRE(loaded.has_value());
    CHECK(live.to_header("sso.buaa.edu.cn") == "SESSION=abc");
    CHECK(loaded->to_header("byxt.buaa.edu.cn") == "TOKEN=xyz");
}

TEST_CASE("CurlCookieStore 拒绝非版本化明文 Cookie blob", "[curl][cookie]") {
    um::CookieJar live;
    UBAANextMocks::MockSecureStore secure_store;
    secure_store.set_string(curl::CurlCookieStore::default_key(), "sso.buaa.edu.cn\t/\tSESSION\tabc\n");
    curl::CurlCookieStore store(live, secure_store);

    auto loaded = store.load();
    REQUIRE_FALSE(loaded.has_value());
    CHECK(loaded.error().code == um::ErrorCode::StorageError);
    CHECK(live.to_header("sso.buaa.edu.cn").empty());
}

TEST_CASE("CurlNetworkStack save_current 持久化当前 live CookieJar 且不重新加载旧值", "[curl][cookie]") {
    UBAANextMocks::MockSecureStore secure_store;
    curl::CurlNetworkStack stack(secure_store);
    auto &store = static_cast<curl::CurlCookieStore &>(stack.cookie_store());

    um::CookieJar stale;
    stale.set_cookie("sso.buaa.edu.cn", "/", "SESSION", "stale");
    REQUIRE(stack.cookie_store().save(stale).has_value());

    store.live_cookies().set_cookie("sso.buaa.edu.cn", "/", "SESSION", "fresh");
    store.live_cookies().set_cookie("byxt.buaa.edu.cn", "/", "TOKEN", "fresh-token");

    auto saved = stack.cookie_store().save_current();
    REQUIRE(saved.has_value());
    CHECK(store.live_cookies().to_header("sso.buaa.edu.cn") == "SESSION=fresh");

    store.live_cookies().clear();
    auto loaded = stack.cookie_store().load();
    REQUIRE(loaded.has_value());
    CHECK(store.live_cookies().to_header("sso.buaa.edu.cn") == "SESSION=fresh");
    CHECK(store.live_cookies().to_header("byxt.buaa.edu.cn") == "TOKEN=fresh-token");
}

TEST_CASE("CurlCookieStore 清理 live jar 和持久化 store", "[curl][cookie]") {
    um::CookieJar live;
    UBAANextMocks::MockSecureStore secure_store;
    curl::CurlCookieStore store(live, secure_store);

    um::CookieJar cookies;
    cookies.set_cookie("sso.buaa.edu.cn", "SESSION", "abc");
    REQUIRE(store.save(cookies).has_value());

    auto cleared = store.clear();
    REQUIRE(cleared.has_value());
    CHECK(live.to_header("sso.buaa.edu.cn").empty());
    CHECK_FALSE(secure_store.get_string(curl::CurlCookieStore::default_key()).has_value());
}

TEST_CASE("CurlNetworkStack 可使用 SecureStore 持久化共享 CookieJar", "[curl][cookie]") {
    UBAANextMocks::MockSecureStore secure_store;
    curl::CurlNetworkStack stack(secure_store);
    auto &store = static_cast<curl::CurlCookieStore &>(stack.cookie_store());

    store.live_cookies().set_cookie("sso.buaa.edu.cn", "SESSION", "abc");
    auto saved = stack.cookie_store().save_current();
    REQUIRE(saved.has_value());

    store.live_cookies().clear();
    auto loaded = stack.cookie_store().load();
    REQUIRE(loaded.has_value());
    CHECK(store.live_cookies().to_header("sso.buaa.edu.cn") == "SESSION=abc");

    auto cleared = stack.cookie_store().clear();
    REQUIRE(cleared.has_value());
    CHECK_FALSE(secure_store.get_string(curl::CurlCookieStore::default_key()).has_value());
}
