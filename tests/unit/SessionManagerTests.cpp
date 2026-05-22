/**
 * @file SessionManagerTests.cpp
 * @brief SessionManager 类的单元测试
 */

#include <UBAANext/Auth/SessionManager.hpp>

#include <UBAANextMocks/MockSecureStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("SessionManager 保存和恢复会话", "[SessionManager]") {
    UBAANextMocks::MockSecureStore store;
    um::SessionManager mgr(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";
    account.access_token = "at_123";
    account.refresh_token = "rt_456";

    auto saved = mgr.save_session("20260000", account, "vpn");
    REQUIRE(saved.has_value());
    REQUIRE(mgr.has_session());
    REQUIRE(mgr.current_username() == "20260000");
    REQUIRE(mgr.connection_mode() == "vpn");
}

TEST_CASE("SessionManager 从存储中恢复", "[SessionManager]") {
    // 使用一个管理器保存，另一个管理器恢复（同一底层存储）
    UBAANextMocks::MockSecureStore store;
    um::SessionManager mgr1(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";
    account.access_token = "at_123";
    account.refresh_token = "rt_456";

    auto saved = mgr1.save_session("20260000", account, "direct");
    REQUIRE(saved.has_value());

    um::SessionManager mgr2(store);
    auto restored = mgr2.restore_session();
    REQUIRE(restored.has_value());
    REQUIRE(restored->student_id == "20260000");
    REQUIRE(restored->display_name == "Test User");
    REQUIRE(restored->access_token == "at_123");
    REQUIRE(mgr2.connection_mode() == "direct");
}

TEST_CASE("SessionManager 清除会话", "[SessionManager]") {
    UBAANextMocks::MockSecureStore store;
    um::SessionManager mgr(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";

    auto saved = mgr.save_session("20260000", account);
    REQUIRE(saved.has_value());
    REQUIRE(mgr.has_session());

    mgr.clear_session();
    REQUIRE_FALSE(mgr.has_session());
    REQUIRE(mgr.current_username().empty());
    REQUIRE(mgr.connection_mode().empty());
}

TEST_CASE("SessionManager 拒绝空用户名并保持存储不变", "[SessionManager]") {
    UBAANextMocks::MockSecureStore store;
    um::SessionManager mgr(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";

    auto saved = mgr.save_session("", account, "vpn");
    REQUIRE_FALSE(saved.has_value());
    CHECK(saved.error().code == um::ErrorCode::InvalidArgument);
    CHECK_FALSE(mgr.has_session());
    CHECK_FALSE(store.get_string("session.active").has_value());
}

TEST_CASE("SessionManager 无会话时恢复返回 nullopt", "[SessionManager]") {
    UBAANextMocks::MockSecureStore store;
    um::SessionManager mgr(store);

    auto restored = mgr.restore_session();
    REQUIRE_FALSE(restored.has_value());
}