# 单元测试

## 概述

UBAA Next 使用 [Catch2](https://github.com/catchorg/Catch2) v3 作为单元测试框架，
通过 CMake FetchContent 引入。

## 测试文件

| 文件 | 测试内容 |
|------|----------|
| `ResultTests.cpp` | Result Ok/Fail 行为 |
| `AuthServiceTests.cpp` | 登录/登出/会话恢复 |
| `CourseServiceTests.cpp` | 课程查询、缓存行为 |
| `ExamServiceTests.cpp` | 考试查询、缓存行为 |
| `ClassroomServiceTests.cpp` | 教室查询、缓存行为 |
| `TermServiceTests.cpp` | 学期/周次查询、缓存行为 |
| `JsonParserTests.cpp` | JSON 解析正确/错误/边界用例 |
| `SessionManagerTests.cpp` | 会话管理 |
| `CookieJarTests.cpp` | Cookie 管理 |
| `SecureStoreAdapterTests.cpp` | 安全存储适配 |
| `CacheStoreTests.cpp` | 缓存 TTL、惰性清除 |

## 运行方式

```powershell
# 配置
cmake --preset windows-msvc-debug

# 构建
cmake --build --preset windows-msvc-debug

# 运行所有测试
ctest --preset windows-msvc-debug

# 运行指定标签的测试
.\build\windows-msvc-debug\tests\Debug\UBAANextTests.exe "[JsonParser]"
```

## Fixture 文件

`tests/fixtures/` 目录包含与 MockHttpClient 内嵌数据一致的 JSON 文件，
可用于未来需要从文件加载测试数据的场景：

- `courses.json` — 课程列表
- `exams.json` — 考试列表
- `classrooms.json` — 教室数据
- `terms.json` — 学期列表
- `weeks.json` — 周次列表

## Mock 注入模式

测试中使用 Mock 实现注入：

```cpp
UBAANextMocks::MockHttpClient http_client;
UBAANextMocks::MockCacheStore cache_store;
CourseService service(http_client, cache_store);
```

`MockHttpClient` 预设了常见 API 路径的 JSON 响应。
`MockCacheStore` 继承自 `MemoryCacheStore`，提供独立的测试类型。

## 缓存集成测试

测试缓存行为的模式：

1. 首次调用 → 触发 HTTP + 写缓存
2. 二次调用 → 命中缓存，不触发 HTTP
3. 验证两次返回结果一致

