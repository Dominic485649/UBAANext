# 依赖规则

## 规则

1. **Core 不得依赖平台 Shell** — CLI、ArkUI、Slint 等应用入口依赖 Core，反之不行。
2. **Core 不得依赖 Mock 实现** — Mocks 依赖 Core，用于离线回归和测试 fixture。
3. **Core 不直接链接平台 SDK、curl 或 OpenSSL** — 真实网络和加密能力由 platform adapter 封装后通过 core 抽象注入。
4. **Core 允许依赖当前必要的跨平台库** — `nlohmann-json` 是 core parser/service/JSON 输出所需的直接依赖。
5. **公共头文件不得包含 `namespace um = UBAANext;`** — 别名仅在 .cpp、测试或 CLI 内部使用。
6. **不使用全局 `include_directories` 或 `link_directories`** — 使用基于 target 的 CMake。
7. **Core 中不得包含 UI 框架依赖** — 不使用 ArkUI、Slint、Windows API 或 Harmony UI API。
8. **测试依赖不得进入 runtime target** — `Catch2` 仅用于测试 target。

## 当前依赖边界

| 能力 | 允许位置 | 禁止位置 |
| --- | --- | --- |
| JSON 解析/序列化 | `UBAANextCore` 通过 `nlohmann-json` | 不应复制第三方源码进 core |
| HTTP/TLS/Cookie | `platform/common/curl` | core service 不直接调用 libcurl API |
| AES/MD5/RSA/sign | `platform/common/openssl` | core service 不直接调用 OpenSSL API |
| DPAPI / Secret Service / Harmony 路径 | `platform/<os>` | core 不包含平台 SDK 头文件 |
| 单元/集成测试 | `tests/*` target | runtime target 不链接 Catch2 |

## 执行方式

- 基于 CMake target 的链接强制执行模块边界。
- 新增依赖必须先说明用途、替代方案、许可证和跨平台影响，并更新 `docs/build/dependency-policy.md` 与 `docs/licensing/third-party-notices.md`。
- 当前真实协议稳定阶段不删除 `curl`、`OpenSSL`、`nlohmann-json` 或 `Catch2`；依赖裁剪需作为独立议题评估。
- 对 core 新增代码做静态检查，避免引入 `_WIN32`、`windows.h`、平台网络 API 或 UI 框架 include。
