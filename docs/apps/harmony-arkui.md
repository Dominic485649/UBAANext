# HarmonyOS ArkUI 应用

## 当前进入阶段

本阶段只进入 ArkUI skeleton / mock-offline 页面规划，不接真实登录 UI，不接真实写 UI，也不让 ArkUI 直接拼接校园系统协议。CLI 仍是 Core、service、parser、platform 和测试的稳定验收入口；ArkUI 只能消费已经在 Core 层定义清楚的 typed service、`FeatureRecord` 兼容结构、错误码和 capability 状态。

## 页面进入顺序

1. **UI skeleton / mock-offline**：可以在本轮之后开始。优先覆盖首页、课表、考试、空教室、Todo、SPOC/Judge 只读列表与详情、BYKC、CGYY、LibrarySeat、YGDK 的只读展示壳。
2. **真实只读 UI**：需要先完成 NAPI/C API 边界合同，包括 typed service 映射、错误码映射、异步 Promise、redaction、capability 查询和 partial failure 展示合同。
3. **真实登录 UI**：必须等 Harmony 平台提供 secure store、cookie persistence、`live_login=true` 和 session 恢复链路，并通过离线与 live 专项测试。
4. **真实写 UI**：本阶段不进入。后续必须继续满足 typed write service、用户二次确认、`write_operations=true`、失败恢复和 live 写专项验证。

## 能力门控

当前 Harmony capability 预期为：

- `real_network = true`
- `redirect_control = true`
- `openssl_crypto = true`
- `app_data_path = true`
- `upload_bytes = true`
- `secure_store = false`
- `cookie_persistence = false`
- `secure_cookie_persistence = false`
- `live_login = false`
- `write_operations = false`

ArkUI 必须展示或处理这些状态，不得把 `UnsupportedSecureStore`、cookie fallback、mock/offline 成功或 CLI 命令存在解释为真实登录/真实写能力已完成。

## 安全边界

- ArkUI 不直接保存 username、password、cookie、token、ticket、session 或 captcha。
- ArkUI 不直接读本地上传文件；上传 bytes 由 Native/platform 边界提供并受 capability 控制。
- ArkUI 不绕过 Core 的错误脱敏、session/cookie 持久化、write gate 或 platform capability。
- 高敏感业务数据只在明确页面中展示，不进入 diagnostics、日志或测试输出。
- `FeatureService` 字符串 `domain/operation` routing 只能作为 mock/offline 和兼容入口，不作为 UI 长期 API。
