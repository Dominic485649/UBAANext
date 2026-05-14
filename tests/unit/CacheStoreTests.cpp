/**
 * @file CacheStoreTests.cpp
 * @brief MemoryCacheStore 类的单元测试
 *
 * 测试覆盖基本 CRUD、覆盖语义和 TTL 过期。
 * TTL 测试使用 1 秒休眠来验证基于时间的清除。
 */

#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

namespace um = UBAANext;

TEST_CASE("MemoryCacheStore 设置和获取", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set("key1", "value1");

    auto val = cache.get("key1");
    REQUIRE(val.has_value());
    REQUIRE(*val == "value1");
}

TEST_CASE("MemoryCacheStore 获取不存在的键返回 nullopt", "[CacheStore]") {
    um::MemoryCacheStore cache;

    auto val = cache.get("missing");
    REQUIRE_FALSE(val.has_value());
}

TEST_CASE("MemoryCacheStore 覆盖写入", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set("key", "old");
    cache.set("key", "new");

    auto val = cache.get("key");
    REQUIRE(val.has_value());
    REQUIRE(*val == "new");
}

TEST_CASE("MemoryCacheStore 移除条目", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set("key", "value");
    cache.remove("key");

    REQUIRE_FALSE(cache.get("key").has_value());
}

TEST_CASE("MemoryCacheStore 清除所有条目", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set("a", "1");
    cache.set("b", "2");
    cache.clear();

    REQUIRE_FALSE(cache.get("a").has_value());
    REQUIRE_FALSE(cache.get("b").has_value());
}

TEST_CASE("MemoryCacheStore TTL 过期", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set_with_ttl("key", "value", 1);

    // 应该立即可用
    auto val = cache.get("key");
    REQUIRE(val.has_value());

    // 等待过期
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // TTL 到期后应该被清除
    auto expired = cache.get("key");
    REQUIRE_FALSE(expired.has_value());
}

TEST_CASE("MemoryCacheStore TTL 未过期", "[CacheStore]") {
    um::MemoryCacheStore cache;
    cache.set_with_ttl("key", "value", 60);

    auto val = cache.get("key");
    REQUIRE(val.has_value());
    REQUIRE(*val == "value");
}