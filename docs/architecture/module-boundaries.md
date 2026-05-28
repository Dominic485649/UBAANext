# 模块边界

## 模块

| 模块 | CMake Target | 角色 |
|------|-------------|------|
| Core | `UBAANextCore` | 业务模型、Result/error、service、parser、协议/session、网络/存储/加密抽象 |
| Mocks | `UBAANextMocks` | 用于测试和离线 CLI 的 Mock 实现 |
| Platform common | `UBAANextCurl` / `UBAANextOpenSSL` 等 | curl 网络栈、Cookie store、OpenSSL crypto provider 等跨平台适配 |
| Platform OS | `platform/windows`、`platform/linux`、`platform/harmony` targets | 安全存储、app-data 路径、平台能力声明 |
| CLI | `ubaa` | 当前主要命令行入口，组合 core 与平台适配 |
| Tests | `UBAANextTests` | 单元测试和集成测试 |

## 依赖规则

```
ubaa (CLI) ──→ UBAANextCore
           ──→ UBAANextMocks（仅 mock/offline 路径）
           ──→ platform adapters

UBAANextTests ──→ UBAANextCore
              ──→ UBAANextMocks
              ──→ platform adapters（按测试需要）
              ──→ Catch2

UBAANextMocks ──→ UBAANextCore

platform adapters ──→ UBAANextCore abstractions
                  ──→ curl / OpenSSL / OS SDK（按 adapter 职责）

UBAANextCore ──→ nlohmann-json
             ──→ C++ 标准库
```

Core 不应直接链接 `CURL::libcurl`、`OpenSSL::Crypto` 或平台 SDK。真实网络、加密和系统能力必须通过抽象接口和 platform adapter 注入。

## 头文件可见性

- **公共头文件**：`core/include/UBAANext/` — 供所有下游 target 使用。
- **Mock 头文件**：`mocks/include/UBAANextMocks/` — 供测试和 mock/offline CLI 路径使用。
- **平台头文件**：`platform/*/include/` — 供应用入口或测试组合平台能力使用，不进入 core 公共 API。
- **内部头文件/实现**：`core/src/` — 不对外暴露给下游 target。

## 当前收口重点

真实协议稳定阶段只做局部边界收口：session、redirect、redaction、错误分类、parser 容错和平台能力声明。真实写操作必须经 typed service 与 `WriteOperationGate`，不能从 `FeatureService` 字符串 routing 或 UI 侧绕过平台 capability。CLI 大拆分、`FeatureService` 大拆分和依赖裁剪应作为后续单独工作处理。
