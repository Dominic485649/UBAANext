# 公共 C++ API

> 当前仓库版本阶段为 `v0.3.0`。公共 C++ API 以 v0.3 的 typed model、parser、service 和 cache 合同为稳定基线；NAPI、真实登录、真实写与平台持久化能力属于后续阶段或受 capability gate 保护的实验路径。

## 范围

公共 C++ API 以 Core typed service 为主，是 CLI、未来 NAPI 和后续上层客户端共用的非 UI 后端边界。CLI 暴露不等于语义完成；service 类存在也不等于真实协议完整。所有 `NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified` 能力必须在 API 注释、测试和文档中保持可见。

## 可作为 UI 前置的只读 API

第一批可进入 NAPI 合同和 ArkUI mock/offline skeleton 的只读能力包括：

- 学期、周次、课表、考试、成绩、教室查询。
- Todo 聚合，只保留来源级 partial failure，不吞成空列表。
- SPOC 列表和单详情；批量详情无原 UBAA 证据，保持 `Unverified`。
- Judge 列表、单详情和批量详情；批量详情保留 `status="error"` 单项记录。
- BYKC `profile/courses/chosen/stats`。
- CGYY `sites/purpose-types/day-info/orders/order detail/lock-code`，其中订单和锁码为高敏感输出。
- LibrarySeat `libraries/areas/seats/reservations/area detail`。
- YGDK `overview/records`。

这些 API 仍需按差异报告区分 `Aligned`、`ReadOnlyCandidate`、`PartiallyMigrated` 和 `Unverified`，不得仅凭 mock/offline 测试宣称真实业务完成。

## 受限写 API

以下能力是远端写或会改变业务状态，必须保持 typed service 与 `WriteOperationGate`：

- BYKC select/unselect/sign。
- CGYY reserve/cancel。
- LibrarySeat book/cancel。
- YGDK submit，包括 `--photo` 本地文件读取与上传 bytes。
- Signin submit。
- Evaluation submit。

调用条件：用户显式确认、平台 `write_operations=true`、service 层 gate 通过。默认平台和默认 live smoke 不执行真实写。

## Placeholder / Unsupported / Fallback

- `file upload` 是稳定占位接口，返回 `NotImplemented`，不读取本地文件，不触发远端请求。
- Harmony `UnsupportedSecureStore` 必须 fail-closed，不允许明文 fallback。
- `UnsupportedCryptoProvider`、`UnsupportedSecureStore`、`VolatileSecureStore`、cookie/session fallback 不能标记为完成。
- `FeatureService` 字符串 routing 仅用于 mock/offline 和兼容聚合，不作为长期 UI API，也不作为真实写入口。

## 输出和错误合同

公共 API 返回的错误消息必须走共享脱敏策略。错误、日志、diagnostics 和测试输出不得泄露 username、password、cookie、token、ticket、session、captcha、authorization、URL query、raw HTML、本地路径、上传文件名、上传 bytes、成绩、锁码、预约、打卡或座位敏感原文。
