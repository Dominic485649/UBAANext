# C API 计划

> 当前仓库版本阶段为 `v0.4.0`。C API 属于路线图 `v0.6 — HarmonyOS NAPI` 的前置工作和实验性 ABI 设计，不属于 v0.4 当前稳定基线。

## 目标

C API 是 DevEco/OpenHarmony NAPI wrapper 之前的稳定 native ABI 层。它只暴露可长期维护的窄接口，不把 C++ service 内部类型、session、cookie、token、write gate 或 parser 实现泄露给 ArkTS/DevEco 项目。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源；鸿蒙 DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 通过 CMake package、exported target 或受控源码子构建复用本项目，不复制 core/service/parser/protocol。

## 当前实现

当前已新增最小 C ABI target：

- target：`UBAANextBindingsC`
- 输出名：`ubaanext_c`
- public header：`bindings/c/include/UBAANext/Bindings/C/UbaaNative.h`
- source：`bindings/c/src/UbaaNative.cpp`

第一批 API 仅包含：

- `ubaanext_version()`：返回 native SDK 版本字符串；不触发远端请求、本地文件读取或写操作。
- `ubaanext_get_capabilities(UbaaNextCapabilities*)`：返回当前平台 capability flags；unsupported、fallback、write-gated 和 unverified 状态不得被后续调用方解释为完成。

## 暂不暴露

本阶段 C API 不暴露：

- 真实登录。
- 真实写操作。
- 通用文件上传。
- `FeatureService::mutate(...)`。
- C++ service 指针或内部模型所有权。
- 原始 cookie/session/token/authorization。
- raw HTML、Location、Set-Cookie、URL query 或本地路径 diagnostics。

## ABI 合同

- C ABI 必须保持窄接口、plain struct、整数错误码和稳定字符串返回。
- 任何可能失败的函数都必须返回稳定错误码，并通过后续错误对象 API 返回已脱敏 message。
- `status="error"` 的 partial failure 记录在后续只读 API 中必须原样传过 C ABI，不得过滤。
- 写操作即使未来暴露，也必须继续满足 typed service、显式确认、`PlatformCapabilities::write_operations` 和 `WriteOperationGate`。
- capability 中 `secure_store=false`、`cookie_persistence=false`、`live_login=false`、`write_operations=false` 必须向 NAPI/ArkTS 透传。

## 与 NAPI 的关系

C ABI 优先冻结语义和二进制边界；NAPI wrapper 只负责 ArkTS 友好的 Promise、对象映射和模块导出，不重新实现协议或脱敏规则。

DevEco 项目第一阶段只能调用 version/capability/mock-offline smoke。真实只读 API 需要在 C ABI 和 NAPI 两层同时补齐错误映射、redaction 和 partial failure contract 后再开放。
