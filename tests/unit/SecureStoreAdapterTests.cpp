/**
 * @file SecureStoreAdapterTests.cpp
 * @brief SecureStoreAdapter 类的单元测试
 */

#include <UBAANext/Storage/SecureStoreAdapter.hpp>

#include <UBAANextMocks/MockSecureStore.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("SecureStoreAdapter 保存和加载账户", "[SecureStoreAdapter]") {
    UBAANextMocks::MockSecureStore store;
    um::SecureStoreAdapter adapter(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";
    account.access_token = "at_123";
    account.refresh_token = "rt_456";

    adapter.save_account(account);

    auto loaded = adapter.load_account();
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->student_id == "20260000");
    REQUIRE(loaded->display_name == "Test User");
    REQUIRE(loaded->access_token == "at_123");
    REQUIRE(loaded->refresh_token == "rt_456");
}

TEST_CASE("SecureStoreAdapter 无数据时加载返回 nullopt", "[SecureStoreAdapter]") {
    UBAANextMocks::MockSecureStore store;
    um::SecureStoreAdapter adapter(store);

    auto loaded = adapter.load_account();
    REQUIRE_FALSE(loaded.has_value());
}

TEST_CASE("SecureStoreAdapter 清除账户", "[SecureStoreAdapter]") {
    UBAANextMocks::MockSecureStore store;
    um::SecureStoreAdapter adapter(store);

    um::Model::Account account;
    account.student_id = "20260000";
    account.display_name = "Test User";

    adapter.save_account(account);
    adapter.clear_account();

    auto loaded = adapter.load_account();
    REQUIRE_FALSE(loaded.has_value());
}