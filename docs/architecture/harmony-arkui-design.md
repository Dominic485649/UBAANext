# Harmony ArkUI 设计

## 当前范围

当前阶段验证 `UBAANextCore` 与 CLI 逻辑在 OpenHarmony Native 工具链下可编译，并冻结 ArkUI 进入前的能力边界。可以开始 UI skeleton / mock-offline 页面规划；真实只读 UI 需要先完成 NAPI/C API 合同；真实登录 UI 需要 secure store、cookie persistence、`live_login` 与 session 恢复链路；真实写 UI 本阶段不进入。

工程分工固定为双项目：`D:\Code\Cpp\UBAANext` 是跨平台 native 真源和 SDK/package 输出方，`D:\Code\OpenHarmony\UBAANext` 是 DevEco/HAP/ArkTS/ArkUI 工程壳。鸿蒙项目必须复用当前项目的 native package、exported target 或受控源码子构建，不复制校园系统协议和 service/parser/protocol。

## Native 边界原则

- ArkTS/NAPI 层只依赖稳定的 Core 服务语义，不直接拼接校园系统协议。
- Core 输出优先使用强类型模型；暂未强类型化的高级服务保持 `FeatureRecord { id, title, status, fields }` 兼容结构。
- `FeatureService` 的字符串 `domain/operation` routing 仅作为 mock/offline 与兼容入口，真实写操作必须走 typed service 和统一写门控，不作为 ArkUI 长期 API。
- 写操作必须延续 CLI 的 `--confirm` 等价安全门，由 ArkUI 显式二次确认后才调用 Core mutation；同时必须通过平台 `write_operations` capability，任一条件缺失都要 fail-closed。
- ArkUI 不直接读写账号、Cookie、token 或下游 session；登录态、Cookie 持久化和 secure store 由 Native/platform 层统一管理，错误输出必须沿用 Core/CLI 的敏感信息脱敏。
- 非 Windows 平台不依赖 WinHTTP、DPAPI、BCrypt；BYKC、LibBook、CGYY 等加密能力后续应统一收敛到平台 `ICryptoProvider`。

## 已验证命令

```powershell
cmake --build "build\\openharmony-clang-debug" --config Debug
```

该构建用于发现非 Windows 分支的 `-Werror` 问题和头文件可移植性问题。当前 CLI 仍是验证载体，后续 NAPI 模块应复用同一套 Core 头文件与服务构造方式。

## ArkUI 迁移建议

- 第一批只做 UI skeleton / mock-offline 页面：首页、课表、考试、空教室、Todo、SPOC/Judge 只读列表与详情、BYKC、CGYY、LibrarySeat、YGDK。
- 真实只读 UI 必须先经过 NAPI/C API 边界合同阶段，明确 typed service 映射、`FeatureRecord` 兼容结构、错误码、异步 Promise、redaction、capability 查询和 partial failure 展示。
- 登录状态、Cookie、缓存应在 Native 侧统一管理，ArkUI 只展示状态和触发动作；Harmony 未提供 secure store、cookie persistence 与 `live_login=true` 前不得接入真实登录 UI。
- 真实写 UI 本阶段不进入；后续必须继续通过 typed write service、ArkUI 二次确认、`write_operations=true`、Core `WriteOperationGate` 和 live 写专项验证。
- 当前 Harmony capability 为 `real_network=true`、`redirect_control=true`、`openssl_crypto=true`、`app_data_path=true`、`upload_bytes=true`，但 `secure_store=false`、`cookie_persistence=false`、`secure_cookie_persistence=false`、`live_login=false`、`write_operations=false`。
- 真实网络和加密能力完成平台抽象前，Harmony 端优先使用 mock/offline 与只读兼容性验证。
- 当前 native SDK 侧已新增最小 C ABI：`UBAANextBindingsC` / `ubaanext_c`，第一阶段只给 DevEco/NAPI smoke 暴露 version 和 capability。`.so` 可加载、HAP 可构建、NAPI smoke 通过都不代表真实后端语义完成。
