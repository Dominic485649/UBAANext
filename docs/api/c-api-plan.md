# C API 计划

> 当前仓库版本阶段为 `v0.4.0`。C API 属于路线图 `v0.6 — HarmonyOS NAPI` 的前置工作和实验性 ABI 设计；`master` 中已有 C ABI 符号用于 Harmony/native bridge 边界收口，不代表真实 UI 或正式 SDK 已发布。

## 目标

C API 是 DevEco/OpenHarmony NAPI wrapper 之前的稳定 native ABI 层。它只暴露可长期维护的窄接口，不把 C++ service 内部类型、session、cookie、token、write gate 或 parser 实现泄露给 ArkTS/DevEco 项目。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源；鸿蒙 DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 应通过 CMake package、exported target 或受控源码子构建复用本项目，不复制 core/service/parser/protocol。

## 当前实现

当前 C ABI target：

- target：`UBAANextBindingsC`
- 输出名：`ubaanext_c`
- public header：`bindings/c/include/UBAANext/Bindings/C/UbaaNative.h`
- source：`bindings/c/src/UbaaNative.cpp`

当前导出的第一批 API 包含：

- 基础与能力：`ubaanext_version()`、`ubaanext_version_info()`、`ubaanext_get_capabilities(UbaaNextCapabilities*)`、`ubaanext_capabilities()`。
- Context 生命周期：`ubaanext_context_create()`、`ubaanext_context_release(...)`、`ubaanext_context_set_connection_mode(...)`、`ubaanext_release_result(...)`。
- 认证/会话实验接口：`ubaanext_auth_login(...)`、`ubaanext_auth_logout(...)`、`ubaanext_auth_restore_session(...)`、`ubaanext_auth_get_session_state(...)`、`ubaanext_auth_whoami(...)`。
- 只读业务接口：`ubaanext_terms(...)`、`ubaanext_weeks(...)`、`ubaanext_courses_today(...)`、`ubaanext_courses_date(...)`、`ubaanext_courses_week(...)`、`ubaanext_grades(...)`、`ubaanext_exams(...)`、`ubaanext_todos(...)`、`ubaanext_signin_today(...)`、`ubaanext_ygdk_overview(...)`、`ubaanext_ygdk_records(...)`。
- 迁移服务 FeatureRecord 通用只读接口：`ubaanext_feature_list(context, domain, operation)`、`ubaanext_feature_show(context, domain, operation, id)`。该接口复用 Core `FeatureService` 路由到已迁移 typed service，可覆盖 `evaluation/list`、`spoc/assignments`、`judge/assignments`、`bykc/profile|courses|chosen|stats`、`cgyy/sites|purpose-types|day-info|orders`、`libbook/libraries|areas|seats|reservations` 等只读投影；未接入或参数缺失的组合必须返回错误 envelope，不得假成功。
- TD 本地只读接口：`ubaanext_td_status(...)`、`ubaanext_td_users(...)`、`ubaanext_td_count_cache(...)`。这些接口只读取本地 TD store 投影，不刷新远端计数、不上传图片、不执行打卡；本地路径和存储错误必须经过统一脱敏 envelope。
- 写性质实验接口：`ubaanext_signin_do(...)`。该接口必须继续通过 `confirmed` 入参、`PlatformCapabilities::write_operations` 和 Core `WriteOperationGate`；UI 侧不得把它视为可直接写能力。

业务 API 统一返回 JSON envelope 字符串：成功为 `{ ok:true, data:{...}, error:null }`，失败为脱敏后的 `{ ok:false, data:null, error:{ code, message } }`。调用方必须使用 `ubaanext_release_result(...)` 释放由 C ABI 返回的 JSON 字符串，不能跨 allocator 调用 `free`。

## C ABI status 合同

非 JSON 辅助函数使用公开 `UbaaNextStatus`：

| status | 数值 | 函数范围 |
| --- | ---: | --- |
| `UBAANEXT_STATUS_OK` | `0` | lifecycle/capability 辅助函数成功。 |
| `UBAANEXT_STATUS_INVALID_ARGUMENT` | `-1` | 空指针或无效参数，例如 `ubaanext_get_capabilities(nullptr)`、空 context。 |
| `UBAANEXT_STATUS_INVALID_CONNECTION_MODE` | `-2` | `ubaanext_context_set_connection_mode(...)` 收到不支持的 mode。 |

业务 API 不使用这些 status 表达失败，仍返回 JSON envelope。后续新增非 JSON 辅助函数必须优先复用这些具名 status，避免裸 `-1/-2` 语义漂移。

## Capability 合同

`UbaaNextCapabilities` 是 C ABI 稳定布局；字段含义与 Core `PlatformCapabilities` 一一对应：

- `real_network`
- `secure_cookie_persistence`
- `cookie_persistence`
- `redirect_control`
- `openssl_crypto`
- `secure_store`
- `app_data_path`
- `upload_bytes`
- `live_login`
- `write_operations`

尾部 `reserved[14]` 为 ABI 兼容扩展预留，写入方必须置零；测试已保护 `write_capabilities(...)` 的清零行为。Capability 是宿主能力声明，不等价于真实登录、真实写 UI 或业务 API 完成；尤其 `write_operations=true` 仍需显式确认和 Core write gate。

## Partial failure 合同

`status="error"` 的 `FeatureRecord` 是有效数据项，用于表达 Todo、Judge 批量详情等聚合只读接口的来源级失败。C ABI serializer 必须原样保留：

- `id`
- `title`
- `status`
- `fields`
- `fields["type"] = "source-error"`
- `fields["errorCode"]`
- `fields["errorMessage"]`
- 必要时的 `fields["submissionStatus"] = "error"`

只要整体请求仍能返回部分数据，JSON envelope 可保持 `ok:true`；调用方不得因为数组中存在 `status="error"` 而丢弃成功项或判定整个 API 失败。

## Mock/offline smoke 合同

启用 `UBAANEXT_BUILD_BINDINGS=ON` 时，测试层新增轻量 C ABI smoke target，覆盖：

- `ubaanext_version()` 返回非空版本字符串；`ubaanext_version_info()` 返回统一 envelope。
- `ubaanext_get_capabilities(nullptr)` 返回 `UBAANEXT_STATUS_INVALID_ARGUMENT`。
- `ubaanext_get_capabilities(&caps)` 返回 `UBAANEXT_STATUS_OK`，且 `reserved` 字节全为 0；`ubaanext_capabilities()` 返回 ArkTS 友好的 camelCase capability envelope。
- `ubaanext_context_create()` / `ubaanext_context_release()` 生命周期可用。
- 非法 mode 返回 `UBAANEXT_STATUS_INVALID_CONNECTION_MODE`。
- 在 mock 构建中，`ubaanext_context_set_connection_mode(context, "mock")` 成功，`ubaanext_terms(context)`、`ubaanext_auth_login(context, ...)`、`ubaanext_auth_whoami(context)`、`ubaanext_todos(context, ...)` 返回稳定 envelope。
- `ubaanext_feature_list(...)` 在 mock mode 下覆盖 evaluation、SPOC、Judge、BYKC、CGYY、LibBook 的只读 FeatureRecord envelope，证明 ArkTS 可通过 Native bridge 消费这些已迁移服务投影而不是解析 CLI 文本。
- 写接口缺少确认时返回错误 envelope，不能执行真实写。
- TD 本地只读接口在空本地 store 下返回 `ok:true` 与空数组，而不是触发远端网络或写操作。

该 smoke 不使用真实账号、真实网络或真实写操作；mock/offline 成功不能解释为真实登录或真实写能力完成。真实登录路径除 mock mode 外还要求平台同时声明 `live_login` 与 `cookie_persistence`，否则返回 `UnsupportedPlatform`，避免 Harmony 等缺少持久化能力的平台假装安全保存。

## 暂不暴露

本阶段 C API 仍不暴露：

- 稳定承诺级真实登录；`ubaanext_auth_login(...)` 在非 mock mode 下若 `live_login=false` 或 `cookie_persistence=false` 必须 fail-closed。
- 通用真实写操作；除已列出的 gated 实验接口外，不暴露 mutation 通道。
- 通用文件上传。
- `FeatureService::mutate(...)` 作为真实写通道；当前仅允许 `ubaanext_feature_list(...)` / `ubaanext_feature_show(...)` 暴露只读投影。
- C++ service 指针或内部模型所有权。
- 原始 cookie/session/token/authorization。
- raw HTML、Location、Set-Cookie、URL query 或本地路径 diagnostics。

## 与 NAPI 的关系

C ABI 优先冻结语义和二进制边界；NAPI wrapper 只负责 ArkTS 友好的 `Promise<T>`、对象映射和模块导出，不重新实现协议或脱敏规则。

DevEco 项目第一阶段只能调用 version/capability/mock-offline smoke 和明确标注的只读实验接口。真实登录 UI 需要等 `secure_store`、`cookie_persistence`、`live_login` 与 session 恢复链路完成；真实写 UI 必须另行完成 typed write service、UI 二次确认、失败恢复和 live 写专项验证。
