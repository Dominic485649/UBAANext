# 项目概览

## 什么是 UBAA Next？

UBAA Next 是 **UBAA（智慧北航 Remake）** 的 C++ 原生重写版本，一个面向北京航空航天大学学生的跨平台校园服务聚合客户端。

原始 UBAA 使用 Kotlin Multiplatform + Compose Multiplatform + Ktor 构建。UBAA Next 旨在用 C++ 重写核心逻辑，以实现：

- **更好的性能** — 原生 C++，最小化运行时开销。
- **更广的平台覆盖** — 以 Windows CLI 为当前入口，后续扩展到 HarmonyOS Native、Linux/嵌入式和桌面 GUI。
- **长期可维护性** — 清晰的 core/platform 边界、可测试协议层、显式安全策略。

## 当前版本阶段

当前仓库版本号为 **0.4.0**，以 [路线图](02-roadmap.md) 的 `v0.4 — CLI 工程化` 为当前基线。该版本阶段覆盖：

- C++ Core 骨架：`Result/Error`、Model、Net/Storage 抽象。
- Mock 实现与离线测试数据。
- AuthService、SessionManager、CookieJar、SecureStore 适配边界。
- JSON 解析、课程/考试/教室/学期/教学周 parser。
- Service 层的 `Mock HTTP → Parser → Cache → Return` 数据流。
- MemoryCacheStore TTL。
- Windows CLI 工程化：稳定命令树、`CommandHandlers` 命令目录、统一 `--json` envelope、固定 exit code `0-6`、配置/缓存子命令。
- CLI integration/golden tests：覆盖 help 合同、JSON 输出、exit code、写操作确认门与关键 mock/offline 命令。

v0.4 已完成 CLI 工程化，但仍保持 mock/offline 优先的稳定边界。master 当前工作区正在收口 UI + CLI 体验，并额外推进北航云盘挂载；相关文档必须把已完成 CLI/Cloud 协议能力、进行中的 UI、未注册或依赖缺失的 mount adapter、以及真实验证要求分开标注。真实 HTTP、系统挂载、平台能力、C ABI、HarmonyOS 和 Slint 相关内容在对应路线图阶段完成并通过真实 smoke 前，不作为当前稳定承诺。

## 架构

UBAA Next 遵循 **C++ Core + Platform Shell** 架构：

- **C++ Core**（`UBAANextCore`）— 业务模型、认证/session、协议解析、服务层、网络/存储抽象。
- **Mock 层** — 用于无需真实校园 API 的测试模拟实现。
- **平台适配层** — 后续阶段承载真实网络、加密、安全存储、Cookie 持久化和系统路径能力。
- **应用入口层** — 调用 C++ Core 的 CLI/UI/绑定层。
  - Windows CLI（`ubaa`）— 当前主要可运行入口。
  - HarmonyOS ArkUI / NAPI — 后续阶段。
  - Windows Slint GUI — 后续阶段。

## 关键设计决策

1. **C++17 默认基线，可选 C++20** — 保证 MSVC 与 OpenHarmony Native clang++ 的可构建性。
2. **命名空间 `UBAANext`** — 公共 API；`um` 别名仅允许在 .cpp/测试/CLI 内部使用。
3. **基于 target 的 Modern CMake** — 不使用全局 include/link 目录。
4. **基于接口的抽象** — `IHttpClient`、`ISecureStore`、`ICacheStore` 等边界先于真实平台实现。
5. **Result 模式** — 显式错误处理，公共 API 不抛异常。
6. **Mock 优先回归** — 默认测试不需要真实账号、密码或校园网络。
7. **真实协议受控启用** — v0.5 及之后涉及 live 凭据或写操作时必须显式 opt-in，并保持 redaction 与确认门。

## 进一步阅读

- [路线图](02-roadmap.md)
- [整体架构](architecture/overall-architecture.md)
- [模块边界](architecture/module-boundaries.md)
- [依赖策略](build/dependency-policy.md)
- [版本号规范](release/versioning.md)
