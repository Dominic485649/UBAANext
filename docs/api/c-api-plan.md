# C API 计划

> 当前仓库版本阶段为 `v0.4.0`。C API 属于路线图 `v0.6 — HarmonyOS NAPI` 的前置工作和实验性 ABI 设计，不属于 v0.4 当前稳定基线；`master` 中已有的 C ABI 符号是 Harmony/native bridge 前置收口，不代表真实 UI 或正式 SDK 已发布。

## 目标

C API 是 DevEco/OpenHarmony NAPI wrapper 之前的稳定 native ABI 层。它只暴露可长期维护的窄接口，不把 C++ service 内部类型、session、cookie、token、write gate 或 parser 实现泄露给 ArkTS/DevEco 项目。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源；鸿蒙 DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 通过 CMake package、exported target 或受控源码子构建复用本项目，不复制 core/service/parser/protocol。

## 当前实现

当前已新增 C ABI target：

- target：`UBAANextBindingsC`
- 输出名：`ubaanext_c`
- public header：`bindings/c/include/UBAANext/Bindings/C/UbaaNative.h`
- source：`bindings/c/src/UbaaNative.cpp`

当前导出的第一批 API 包含：

- 基础与能力：`ubaanext_version()`、`ubaanext_get_capabilities(UbaaNextCapabilities*)`。
- Context 生命周期：`ubaanext_context_create()`、`ubaanext_context_release(...)`、`ubaanext_context_set_connection_mode(...)`、`ubaanext_release_result(...)`。
- 认证/会话实验接口：`ubaanext_auth_login(...)`、`ubaanext_auth_logout(...)`、`ubaanext_auth_restore_session(...)`、`ubaanext_auth_get_session_state(...)`。
- 只读业务接口：`ubaanext_terms(...)`、`ubaanext_weeks(...)`、`ubaanext_courses_today(...)`、`ubaanext_courses_date(...)`、`ubaanext_courses_week(...)`、`ubaanext_grades(...)`、`ubaanext_exams(...)`、`ubaanext_todos(...)`、`ubaanext_signin_today(...)`、`ubaanext_ygdk_overview(...)`、`ubaanext_ygdk_records(...)`。
- 写性质实验接口：`ubaanext_signin_do(...)`。该接口必须继续通过 `confirmed` 入参、`PlatformCapabilities::write_operations` 和 Core `WriteOperationGate`，不能被 UI 侧视为可直接写。

这些 API 统一返回 JSON envelope 字符串：成功为 `ok/data/error`，失败为脱敏后的 `error.code` / `error.message`。调用方必须使用 `ubaanext_release_result(...)` 释放由 C ABI 返回的 JSON 字符串。

## 暂不暴露

本阶段 C API 仍不暴露：

- 稳定承诺级真实登录；`ubaanext_auth_login(...)` 在 `live_login=false` 时必须 fail-closed。
- 通用真实写操作；除已列出的 gated 实验接口外，不暴露 mutation 通道。
- 通用文件上传。
- `FeatureService::mutate(...)`。
- C++ service 指针或内部模型所有权。
- 原始 cookie/session/token/authorization。
- raw HTML、Location、Set-Cookie、URL query 或本地路径 diagnostics。

## ABI 合同

- C ABI 必须保持窄接口、plain struct、整数错误码和稳定字符串返回。
- 任何可能失败的函数都必须返回稳定错误码，并通过 JSON envelope 返回已脱敏 message。
- `status="error"` 的 partial failure 记录在只读 API 中必须原样传过 C ABI，不得过滤。
- 写操作即使未来扩展，也必须继续满足 typed service、显式确认、`PlatformCapabilities::write_operations` 和 `WriteOperationGate`。
- capability 中 `secure_store=false`、`cookie_persistence=false`、`live_login=false` 以及当前平台的 `write_operations` 实际值必须向 NAPI/ArkTS 透传。
- `UbaaNextCapabilities` 尾部保留 `reserved[14]`，写入方必须置零；后续只允许在不破坏现有字段偏移的前提下使用预留字节或追加兼容字段。

## 与 NAPI 的关系

C ABI 优先冻结语义和二进制边界；NAPI wrapper 只负责 ArkTS 友好的 Promise、对象映射和模块导出，不重新实现协议或脱敏规则。

DevEco 项目第一阶段只能调用 version/capability/mock-offline smoke 和明确标注的只读实验接口。真实登录 UI 需要等 `secure_store`、`cookie_persistence`、`live_login` 与 session 恢复链路完成；真实写 UI 必须另行完成 typed write service、UI 二次确认、失败恢复和 live 写专项验证。
