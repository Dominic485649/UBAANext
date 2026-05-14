# 项目概览

## 什么是 UBAA Next？

UBAA Next 是 **UBAA（智慧北航 Remake）** 的 C++ 原生重写版本，一个面向北京航空航天大学学生的跨平台校园服务聚合客户端。

原始 UBAA 使用 Kotlin Multiplatform + Compose Multiplatform + Ktor 构建。UBAA Next 旨在用 C++ 重写核心逻辑，以实现：

- **更好的性能** — 原生 C++，最小化运行时开销
- **更广的平台覆盖** — HarmonyOS Native、嵌入式系统，以及没有 JVM/Dalvik 的平台
- **长期可维护性** — 稳定的 ABI 边界、清晰的模块分离、全面的测试

## 架构

UBAA Next 遵循 **C++ Core + Platform Shell** 架构：

- **C++ Core**（`UBAANextCore`）— 所有业务逻辑、数据模型、网络/存储抽象
- **Mock 层**（`UBAANextMocks`）— 用于无需真实校园 API 的测试模拟实现
- **平台 Shell** — 调用 C++ Core 的薄 UI/入口层：
  - Windows CLI（`ubaa`）— v0.1
  - HarmonyOS ArkUI（通过 NAPI）— v0.5/v0.6
  - Windows Slint GUI — v0.7

## 当前状态

**v0.1** — C++ Core 骨架

这是第一个里程碑，专注于：

- 建立 C++17/C++20 + CMake 项目结构
- 定义核心抽象（Result/Error、Model、Net、Storage）
- AuthService 和 CourseService 的 Mock 实现
- Windows CLI 入口
- OpenHarmony Native clang++ 可构建路径
- 通过 Catch2 进行单元测试

无真实校园 API 集成，无真实认证，无 UI 框架。

## 关键设计决策

1. **C++17/C++20 基线** — 默认 C++17，可选 C++20，已验证 MSVC 与 OpenHarmony clang++ 构建
2. **命名空间 `UBAANext`** — 公共 API；`um` 别名仅允许在 .cpp/测试/CLI 内部使用
3. **基于 Target 的 Modern CMake** — 不使用全局 include/link 目录
4. **基于接口的抽象** — `IHttpClient`、`ISecureStore`、`ICacheStore`
5. **Result 模式** — 显式错误处理，公共 API 不抛异常
6. **Mock 优先开发** — v0.1 中所有外部依赖使用 mock
