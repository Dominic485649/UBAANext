# 项目概览

## 什么是 UBAA Next？

UBAA Next 是 **UBAA（智慧北航 Remake）** 的 C++ 原生重写版本，一个面向北京航空航天大学学生的跨平台校园服务聚合客户端。

原始 UBAA 使用 Kotlin Multiplatform + Compose Multiplatform + Ktor 构建。UBAA Next 旨在用 C++ 重写核心逻辑，以实现：

- **更好的性能** — 原生 C++，最小化运行时开销
- **更广的平台覆盖** — Windows CLI、HarmonyOS Native、Linux/嵌入式等没有 JVM/Dalvik 的平台
- **长期可维护性** — 清晰的 core/platform 边界、可测试协议层、显式安全策略

## 架构

UBAA Next 遵循 **C++ Core + Platform Shell** 架构：

- **C++ Core**（`UBAANextCore`）— 业务模型、认证/session、协议解析、服务层、网络/存储抽象
- **Mock 层**（`UBAANextMocks`）— 用于无需真实校园 API 的测试模拟实现
- **平台适配层** — 提供真实网络、加密、安全存储、Cookie 持久化和系统路径能力
  - `platform/common/curl` — libcurl 网络栈
  - `platform/common/openssl` — OpenSSL crypto provider
  - `platform/windows` — Windows DPAPI、能力声明和路径适配
  - `platform/linux` — Linux 能力声明和可选 Secret Service
  - `platform/harmony` — HarmonyOS 能力声明和路径适配
- **应用入口层** — 调用 C++ Core 的 CLI/UI/绑定层
  - Windows CLI（`ubaa`）— 当前主要可运行入口
  - HarmonyOS ArkUI（通过 NAPI）— 未来 UI 入口
  - Windows Slint GUI — 未来 UI 入口

## 当前状态

当前项目已经不再是 v0.1 mock-only 骨架。代码库已包含：

- C++17 默认构建，可选 C++20 验证
- Windows CLI、JSON 输出、配置管理和离线 mock 数据
- 真实 SSO/UC 登录、session 恢复、BYXT/app.buaa 等下游 session 激活基础
- 课表、考试、空教室、学期/周次、成绩、SPOC、Judge、Todo、签到、阳光打卡、博雅课程、场馆预约、图书馆座位、评教等 service/CLI 入口
- curl/OpenSSL 平台适配、DPAPI/Secret Service/Harmony 能力边界
- 单元测试、集成测试和可选 live smoke gates

仍需继续打磨的部分包括：真实协议稳定性、各业务 parser 对字段漂移的容错、跨系统 session 过期语义、写操作 live 验收和正式原 UBAA 功能对照表。

## 关键设计决策

1. **C++17 默认基线，可选 C++20** — 保证 MSVC 与 OpenHarmony Native clang++ 的稳定可构建性。
2. **命名空间 `UBAANext`** — 公共 API；`um` 别名仅允许在 .cpp/测试/CLI 内部使用。
3. **基于 target 的 Modern CMake** — 不使用全局 include/link 目录。
4. **基于接口的抽象** — `IHttpClient`、`INetworkStack`、`ISecureStore`、`ICacheStore`、`ICryptoProvider`。
5. **Result 模式** — 显式错误处理，公共 API 不抛异常。
6. **Mock 优先回归** — 默认测试不需要真实账号、密码或校园网络。
7. **真实协议受控启用** — 涉及 live 凭据或写操作时必须显式 opt-in，并保持 redaction 与确认门；真实写操作还必须通过平台 `write_operations` capability，默认 fail-closed。

## 进一步阅读

- [整体架构](architecture/overall-architecture.md)
- [模块边界](architecture/module-boundaries.md)
- [Core 跨平台性与保守清理报告](reports/core-portability-and-cleanup-report.md)
- [原 UBAA 后端差异报告](reports/original-ubaa-backend-feature-matrix.md)
- [依赖策略](build/dependency-policy.md)
