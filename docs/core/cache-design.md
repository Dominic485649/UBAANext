# 缓存设计

## 概述

缓存层用于减少重复的 HTTP 请求，提升响应速度。
通过 `ICacheStore` 接口抽象，支持内存型和未来持久型实现。

## 接口定义

`ICacheStore`（`core/include/UBAANext/Storage/CacheStore.hpp`）：

| 方法 | 说明 |
|------|------|
| `set(key, value)` | 存储值（无过期） |
| `get(key)` → `optional<string>` | 获取值，不存在或过期返回 nullopt |
| `remove(key)` | 移除指定条目 |
| `clear()` | 清除所有条目 |

## TTL 设计

`MemoryCacheStore` 扩展了 `ICacheStore`，支持 `set_with_ttl(key, value, ttl_seconds)`：

- 使用 `std::chrono::steady_clock` 进行单调时间追踪
- 过期条目在 `get()` 时惰性清除（不主动定时器）
- TTL <= 0 等同于无过期存储

## 缓存键约定

| 数据类型 | 缓存键格式 | 示例 |
|----------|-----------|------|
| 今日课程 | `cache:course:today` | — |
| 周课程 | `cache:course:week:<N>` | `cache:course:week:8` |
| 考试列表 | `cache:exam:list` | — |
| 教室查询 | `cache:classroom:<campus>:<date>` | `cache:classroom:1:2026-05-12` |
| 学期列表 | `cache:term:list` | — |
| 周次列表 | `cache:week:<termCode>` | `cache:week:2025-2026-2` |

## TTL 默认值

所有缓存条目默认 TTL = 300 秒（5 分钟）。

## 线程安全

`MemoryCacheStore` **非线程安全**。多线程使用需外部同步。
当前 v0.4 仍在单线程 CLI 和单元测试中使用，无需加锁。

## Service 层缓存集成

各 Service 的缓存集成模式：

1. 先查缓存 → 命中则直接解析返回
2. 缓存未命中 → 发 HTTP 请求 → 解析 → 写入缓存 → 返回
3. 写缓存时优先使用 `set_with_ttl()`（如果 cache 是 `MemoryCacheStore` 类型）
4. 否则退化为 `set()`（无 TTL）

```cpp
auto *mc = dynamic_cast<MemoryCacheStore *>(&m_cache);
if (mc) {
    mc->set_with_ttl(key, body, 300);
} else {
    m_cache.set(key, body);
}
```

## 未来扩展

- v0.5+：SQLite 持久化缓存
- v0.5+：缓存失效策略（手动清除、网络错误重试）
- v0.6+：后台数据刷新

