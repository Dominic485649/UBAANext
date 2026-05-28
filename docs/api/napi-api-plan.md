# NAPI API 计划

## 目标

NAPI 层是 Harmony ArkUI 调用 UBAANext Core 的边界，不重新实现校园系统协议，不直接处理 cookie/session/token 持久化，也不绕过 Core 的 capability gate 和错误脱敏。真实只读 UI 进入前，必须先冻结本文件中的 API 合同。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源，DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 只负责鸿蒙工程、HAP、Ability 和 ArkTS/ArkUI。NAPI wrapper 应复用当前项目输出的 C ABI/native SDK/package，不复制 core/service/parser/protocol。

## API 分层

### Capability 查询

提供只读 capability 查询接口，返回当前平台能力：`real_network`、`redirect_control`、`openssl_crypto`、`secure_store`、`cookie_persistence`、`secure_cookie_persistence`、`live_login`、`write_operations`、`app_data_path`、`upload_bytes`。

ArkTS 页面必须根据 capability 显示不可用状态；不得把 mock/offline 成功视为真实能力完成。

### Typed readonly service

第一批 NAPI 只暴露只读 typed service 或稳定只读投影：

- 学期、周次、课表、考试、成绩、教室查询。
- Todo 聚合列表，保留来源级 `status=error` 记录。
- SPOC assignments 和单 assignment detail；批量详情继续保持 `Unverified`，不在 NAPI 中伪造。
- Judge assignments、单详情和批量详情；批量详情必须保留 partial failure 记录。
- BYKC profile/courses/chosen/stats。
- CGYY sites/purpose-types/day-info/orders/order detail/lock-code。
- LibrarySeat libraries/areas/seats/reservations/area detail。
- YGDK overview/records。

### 兼容 `FeatureRecord`

暂未强类型化或聚合类接口使用兼容结构：

```ts
interface FeatureRecord {
  id: string;
  title: string;
  status: string;
  fields: Record<string, string>;
}
```

`status="error"` 是有效单项记录，不等同于列表整体失败。NAPI 不得过滤掉这类记录。

### 错误映射

所有 Core `ErrorCode` 映射为稳定 ArkTS 错误对象：

```ts
interface UbaaError {
  code: string;
  message: string;
}
```

`message` 必须来自 Core redaction 后的文本；NAPI 不输出 raw body、URL query、Authorization、Set-Cookie、本地路径、上传文件名、成绩、锁码、预约、打卡或座位敏感原文到 diagnostics。

### 异步模型

所有可能触发网络、文件或 secure store 的 API 均返回 `Promise<T>`。NAPI 内部不得阻塞 ArkUI 主线程；取消、超时和 session expired 必须映射为稳定错误码。

### 写操作

真实写 API 本阶段不暴露给 ArkUI。后续如暴露，必须同时满足：

- typed write service。
- ArkUI 显式二次确认。
- `write_operations=true`。
- Core `WriteOperationGate` 通过。
- live 写专项测试已覆盖。

`FeatureService::mutate(...)` 不作为 NAPI 真实写入口。

## 当前结论

本轮后可以开始 ArkUI skeleton / mock-offline 页面规划；真实只读 UI 需要先完成并验证 C ABI + NAPI 合同；真实登录 UI 与真实写 UI 暂不进入。

当前 C ABI 已提供最小 `UBAANextBindingsC` / `ubaanext_c` 骨架，只暴露 `ubaanext_version()` 和 `ubaanext_get_capabilities(...)`。NAPI 第一阶段应只封装 version/capability/mock-offline smoke，后续再逐步开放 typed readonly service。
