/**
 * @file AuthServiceTests.cpp
 * @brief AuthService 类的单元测试
 */

#include <UBAANext/Auth/AuthService.hpp>

#include <UBAANextMocks/MockHttpClient.hpp>
#include <UBAANextMocks/MockSecureStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("AuthService 模拟登录使用有效凭据成功", "[AuthService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockSecureStore secure_store;
    um::AuthService auth(http_client, secure_store);

    auto result = auth.login_mock("20260000", "test");
    REQUIRE(result.has_value());
    REQUIRE(result->student_id == "20260000");
    REQUIRE(result->display_name == "Test User");
    REQUIRE(auth.has_session());
}

TEST_CASE("AuthService 使用空用户名登录失败", "[AuthService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockSecureStore secure_store;
    um::AuthService auth(http_client, secure_store);

    auto result = auth.login_mock("", "test");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("AuthService 使用空密码登录失败", "[AuthService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockSecureStore secure_store;
    um::AuthService auth(http_client, secure_store);

    auto result = auth.login_mock("20260000", "");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("AuthService 登出清除会话", "[AuthService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockSecureStore secure_store;
    um::AuthService auth(http_client, secure_store);

    (void)auth.login_mock("20260000", "test");
    REQUIRE(auth.has_session());

    auto result = auth.logout();
    REQUIRE(result.has_value());
    REQUIRE_FALSE(auth.has_session());
}

TEST_CASE("AuthService 恢复会话时同步连接模式", "[AuthService]") {
    UBAANextMocks::MockHttpClient http_client;
    UBAANextMocks::MockSecureStore secure_store;

    um::AuthService auth1(http_client, secure_store);
    auth1.set_connection_mode(um::ConnectionMode::Direct);
    auto login = auth1.login_mock("20260000", "test");
    REQUIRE(login.has_value());

    um::AuthService auth2(http_client, secure_store);
    auth2.set_connection_mode(um::ConnectionMode::WebVPN);
    auto restored = auth2.restore_session();
    REQUIRE(restored.has_value());
    REQUIRE(auth2.connection_mode() == um::ConnectionMode::Direct);
}
