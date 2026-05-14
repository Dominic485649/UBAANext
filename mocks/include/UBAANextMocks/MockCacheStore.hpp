/**
 * @file MockCacheStore.hpp
 * @brief 用于单元测试的模拟缓存存储
 *
 * 继承自 MemoryCacheStore——功能完全相同，
 * 但提供了一个独立的类型用于测试中的 Mock 注入。
 */
#pragma once

#include <UBAANext/Storage/MemoryCacheStore.hpp>

namespace UBAANextMocks {

/// @brief 模拟缓存存储（MemoryCacheStore 的薄封装）
class MockCacheStore : public UBAANext::MemoryCacheStore {
};

} // namespace UBAANextMocks
