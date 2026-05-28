# 错误码注册表 (Error Codes Registry)

> 当前仓库版本阶段为 `v0.4.0`。错误码合同用于 parser/service/cache 与 CLI mock/offline 验收，并固定 CLI exit code `0-6`；v0.5+ 的真实网络、C ABI、NAPI 和 UI 壳应继续复用该映射。

本篇文档详细列出 `UBAANext` 原生核心库及其 C ABI 所采用的统一错误模型和完整的错误码分类注册表。

## 1. 统一错误模型概述

在 `UBAANextCore` 内部，任何可能发生预期失败的底层或业务函数均不使用 C++ 异常，而是统一返回 monadic 包装结构 `Result<T>`。其定义于 `UBAANext::Base::Result.hpp`，失败状态下内部封装了标准的 `UBAANext::Error` 结构：

```cpp
struct Error {
    ErrorCode code = ErrorCode::None;  ///< 错误分类标识
    std::string message;               ///< 人类可读且已脱敏的错误描述
};
```

---

## 2. 错误码注册表

以下是声明在 [Error.hpp](file:///d:/Code/Cpp/UBAANext/core/include/UBAANext/Base/Error.hpp) 中强类型枚举 `ErrorCode` 的完整定义，以及对应的 C ABI 字符串表现与业务上下文：

| 错误码枚举值 (ErrorCode) | ABI 字符串名称 | 对应的 CLI 退出码 | 详细说明与产生条件 |
| :--- | :--- | :--- | :--- |
| `None` | `"None"` | `0` | 无错误，表示操作成功完成。 |
| `Unknown` | `"Unknown"` | `1` | 未分类错误。通常用作未知第三方库抛出异常或系统内部未定义边界的兜底值。 |
| `InvalidArgument` | `"InvalidArgument"` | `2` | 参数无效。调用方提供的输入参数无效（例如空指针、空密码、越界的页数或格式非法的日期等）。 |
| `NetworkError` | `"NetworkError"` | `4` | HTTP 请求或底层连接失败。通常表示物理网络不通、DNS 解析超时或目标校园服务器断开连接。 |
| `AuthFailed` | `"AuthFailed"` | `3` | 认证或凭据错误。表示输入的用户名密码不正确、验证码识别错误或 SSO 校验票据（Ticket）失败。 |
| `SessionExpired` | `"SessionExpired"` | `3` | 会话已过期。表示存储于 Local Store 中的 Token/Cookie 已经失效，需提示用户重新执行登录流。 |
| `ParseError` | `"ParseError"` | `5` | 响应体解析失败。表示由于学校前端服务器结构调整导致 HTML 页面解析出错，或是返回的 JSON 结构发生了字段漂移（Field Drift）。 |
| `UnsupportedPlatform` | `"UnsupportedPlatform"` | `1` | 平台不支持。当前宿主平台不满足该操作的基本系统环境要求。 |
| `UnsupportedNetwork` | `"UnsupportedNetwork"` | `4` | 平台未接入网络能力。例如当前构建强行关闭了真实的 libcurl 依赖，导致网络请求路径断路。 |
| `UnsupportedSecureStore` | `"UnsupportedSecureStore"` | `6` | 平台未接入安全存储。通常指当前平台缺乏原生凭据箱，且为了数据合规强行阻断了明文 fallback 持久化。 |
| `UnsupportedCrypto` | `"UnsupportedCrypto"` | `1` | 平台未接入加密引擎。指由于未链接 OpenSSL 加密模块而无法进行加解密计算。 |
| `UnsupportedCookiePersistence` | `"UnsupportedCookiePersistence"` | `6` | 平台未接入 Cookie 持久化。无法在磁盘级别持久保存 Cookie 会话。 |
| `Timeout` | `"Timeout"` | `4` | 操作超时。网络会话或平台底层操作在规定时间内没有得到响应。 |
| `TlsError` | `"TlsError"` | `4` | TLS/证书校验失败。本地与校园网站网关握手失败，常发生于系统时间异常或根证书信任链缺失。 |
| `CryptoError` | `"CryptoError"` | `1` | 加密、摘要或签名运算失败。常指 AES/RSA 加解密过程出现密钥尺寸不匹配或格式损毁。 |
| `StorageError` | `"StorageError"` | `6` | 本地存储读写失败。通常表示对本地缓存文件或会话凭据文件的物理读写权限不足。 |
| `NotImplemented` | `"NotImplemented"` | `1` | 功能尚未实现。指功能在 API 层预留占位，会在未来的迭代版本中补全（如文件上传占位等）。 |

---

## 3. C ABI 错误序列化表现

当错误穿越 C ABI 边界时，其在返回的 JSON 字符串中以 `error` 包装对象的形式呈现。该对象的 `code` 节点值恒为上表中的 **ABI 字符串名称**，便于上层快速消费：

```json
{
  "ok": false,
  "data": null,
  "error": {
    "code": "SessionExpired",
    "message": "会话已过期，请重新登录"
  }
}
```

---

## 4. 客户端处理建议

### 4.1 CLI 应用程序
CLI 工具在捕获错误并向控制台输出后，应严格按照上表中的 **CLI 退出码** 调用 `exit(code)`。上层脚本或 CI 执行器（如 `live-smoke.ps1`）可以通过退出码快速识别是**认证类失败 (Code 3)** 还是**网络类失败 (Code 4)**。

### 4.2 UI/宿主应用程序 (如 HarmonyOS 侧)
1. **优先识别 `SessionExpired` 与 `AuthFailed`**：遇到此二者应立即清除本地 UI 侧的登录状态，自动重定向至登录界面，阻止其他后续的接口调用。
2. **优雅降级 `StorageError` 与 `UnsupportedSecureStore`**：如果返回此类错误，宿主侧应允许用户在非持久化会话（即仅在当前冷启动运行生命周期内保持登录态）的环境下继续使用，但需要进行一次弹窗警告以提供足够的信息透明度。
3. **展示局部失败 (Partial Failure)**：在 Todo 服务等包含多个子系统聚合的 API 中，如果个别子系统返回了 `ParseError` 或 `NetworkError`，客户端应采用“只读非阻塞降级”，在列表末尾或角落提示部分组件数据拉取失败及错误码，而不应直接崩溃或渲染白屏。
