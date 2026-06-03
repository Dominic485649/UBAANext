# 错误码注册表（Error Codes Registry）

> 当前仓库版本阶段为 `v0.4.0`。Core `ErrorCode` / `error_code_to_string(...)` 是唯一错误字符串来源；CLI、C ABI JSON、未来 NAPI/ArkTS `UbaaError` 都必须复用该字符串，不维护第二套错误码表。

## 1. 统一错误模型

Core 中可预期失败使用 `Result<T>` 表达，失败值为 `UBAANext::Error`：

```cpp
struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;
};
```

`message` 必须在跨出 Core、CLI、C ABI 或 NAPI 边界前完成脱敏；不得包含 password、cookie、token、ticket、Authorization、Set-Cookie、URL query、raw HTML、本地路径、上传文件名、锁码、预约、打卡或座位敏感原文。

## 2. Core → CLI / C ABI / NAPI 映射

| Core `ErrorCode` | 稳定字符串 / C ABI JSON `error.code` / ArkTS `UbaaError.code` | CLI exit code | 语义 |
| --- | --- | ---: | --- |
| `None` | `None` | `0` | 无错误。 |
| `Unknown` | `Unknown` | `1` | 未分类兜底错误。 |
| `InvalidArgument` | `InvalidArgument` | `2` | 参数无效、缺少必需参数、格式越界或 C ABI 空指针。 |
| `NetworkError` | `NetworkError` | `4` | HTTP、DNS、连接或底层网络失败。 |
| `AuthFailed` | `AuthFailed` | `3` | 用户名密码、验证码、票据或认证流程失败。 |
| `SessionExpired` | `SessionExpired` | `3` | 已保存会话失效，需要重新登录。 |
| `ParseError` | `ParseError` | `5` | JSON/HTML/字段结构解析失败。 |
| `UnsupportedPlatform` | `UnsupportedPlatform` | `1` | 当前宿主平台不支持该能力。 |
| `UnsupportedNetwork` | `UnsupportedNetwork` | `4` | 当前构建或平台未接入真实网络能力。 |
| `UnsupportedSecureStore` | `UnsupportedSecureStore` | `6` | 当前平台未接入安全存储能力。 |
| `UnsupportedCrypto` | `UnsupportedCrypto` | `1` | 当前平台未接入真实加密能力。 |
| `UnsupportedCookiePersistence` | `UnsupportedCookiePersistence` | `6` | 当前平台未接入安全 Cookie 持久化能力。 |
| `Timeout` | `Timeout` | `4` | 网络或平台操作超时。 |
| `TlsError` | `TlsError` | `4` | TLS、证书或握手失败。 |
| `CryptoError` | `CryptoError` | `1` | 加密、摘要或签名运算失败。 |
| `StorageError` | `StorageError` | `6` | 安全存储、缓存或本地状态读写失败。 |
| `NotImplemented` | `NotImplemented` | `1` | 功能预留但尚未实现。 |

CLI 退出码定义见 `apps/cli/include/ExitCodes.hpp`；业务命令映射应通过 `map_error_to_exit_code(...)` 保持与上表一致。

## 3. C ABI JSON 错误合同

C ABI 业务 API 返回 `const char *` JSON envelope。失败时固定为：

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

`error.code` 必须来自 `error_code_to_string(...)`；`error.message` 必须已脱敏。调用方必须用 `ubaanext_release_result(...)` 释放 C ABI 返回的 JSON 字符串。

## 4. C ABI 非 JSON status

少数 lifecycle/capability 辅助函数不返回 JSON，而是返回 `UbaaNextStatus`：

| C ABI status | 数值 | 用途 |
| --- | ---: | --- |
| `UBAANEXT_STATUS_OK` | `0` | 辅助函数成功。 |
| `UBAANEXT_STATUS_INVALID_ARGUMENT` | `-1` | 传入空指针或无效参数，例如 `ubaanext_get_capabilities(nullptr)`。 |
| `UBAANEXT_STATUS_INVALID_CONNECTION_MODE` | `-2` | `ubaanext_context_set_connection_mode(...)` 收到不支持的 mode。 |

`UbaaNextStatus` 只用于非 JSON 辅助函数；业务 API 失败仍通过 JSON envelope 表达。

## 5. Partial failure 展示合同

聚合类只读接口不应因单一来源失败而整体失败。若主请求可返回部分数据，整体 envelope 仍为 `ok:true`，失败来源作为 `FeatureRecord` 单项展示：

```json
{
  "id": "judge:details:source-error",
  "title": "Judge 批量详情",
  "status": "error",
  "fields": {
    "type": "source-error",
    "errorCode": "NetworkError",
    "errorMessage": "下游服务暂不可用",
    "submissionStatus": "error"
  }
}
```

CLI、C ABI 和未来 NAPI/ArkTS 不得过滤 `status:"error"` 的记录；ArkUI 应把它当作可展示数据项，而不是把整个 Promise 或页面渲染判定为失败。
