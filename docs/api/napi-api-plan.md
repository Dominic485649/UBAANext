# NAPI API 计划

> 当前仓库版本阶段为 `v0.4.0`。NAPI 属于路线图 `v0.6 — HarmonyOS NAPI` 后续计划，本页是边界设计草案，不代表 v0.4 当前稳定 API。

## 目标

NAPI 层是 Harmony ArkUI 调用 UBAANext Core 的边界。它不重新实现校园系统协议，不直接处理 cookie/session/token 持久化，也不绕过 Core capability gate、write gate 和错误脱敏。真实只读 UI 进入前，必须先冻结本文件中的 API 合同并通过 C ABI / NAPI smoke。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源；DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 只负责鸿蒙工程、HAP、Ability 和 ArkTS/ArkUI。NAPI wrapper 应复用当前项目输出的 C ABI/native SDK/package，不复制 core/service/parser/protocol。

## 当前 native 基线

当前 C ABI 已不只是 version/capability 骨架，`UBAANextBindingsC` / `ubaanext_c` 已包含：

- version 与 capability。
- context 生命周期和 connection mode。
- 认证/会话实验接口。
- 学期、周次、课表、成绩、考试、Todo、签到今日、YGDK 概览/记录等只读实验接口。
- gated 的 `signin do` 实验接口。

NAPI 第一阶段应从这些 C ABI/native SDK 能力上做薄封装；不得在 ArkTS 侧重写协议、parser、redaction 或 write gate。

## ArkTS 基础类型

### Envelope 与错误

C ABI 业务 API 返回 JSON envelope；NAPI 可选择把失败 envelope 转为 rejected `Promise<UbaaError>`，但错误对象必须保留 Core 字符串：

```ts
interface UbaaEnvelope<T> {
  ok: boolean;
  data: T | null;
  error: UbaaError | null;
}

interface UbaaError {
  code: string;
  message: string;
}
```

`code` 必须来自 Core `error_code_to_string(...)`，例如 `InvalidArgument`、`SessionExpired`、`NetworkError`、`ParseError`、`StorageError`。`message` 必须来自 Core/C ABI redaction 后的文本；NAPI 不输出 raw body、URL query、Authorization、Set-Cookie、本地路径、上传文件名、成绩、锁码、预约、打卡或座位敏感原文到 diagnostics。

### Capability

ArkTS 使用 camelCase 字段：

```ts
interface UbaaCapabilities {
  realNetwork: boolean;
  secureCookiePersistence: boolean;
  cookiePersistence: boolean;
  redirectControl: boolean;
  opensslCrypto: boolean;
  secureStore: boolean;
  appDataPath: boolean;
  uploadBytes: boolean;
  liveLogin: boolean;
  writeOperations: boolean;
}
```

`getCapabilities(): UbaaCapabilities` 必须是本地只读查询，不触发真实网络、登录或写操作。ArkTS 页面必须根据 capability 显示不可用状态；不得把 mock/offline 成功、C ABI 符号存在或 CLI 命令存在解释为真实能力完成。

### FeatureRecord 与 partial failure

暂未强类型化或聚合类接口使用兼容结构：

```ts
interface FeatureRecord {
  id: string;
  title: string;
  status: string;
  fields: Record<string, string>;
}
```

`status="error"` 是有效单项记录，不等同于列表整体失败。典型来源级失败字段：

```ts
{
  id: 'judge:details:source-error',
  title: 'Judge 批量详情',
  status: 'error',
  fields: {
    type: 'source-error',
    errorCode: 'NetworkError',
    errorMessage: '下游服务暂不可用',
    submissionStatus: 'error'
  }
}
```

NAPI 不得过滤掉这类记录；ArkUI 应将其展示为局部失败提示，同时保留同一列表中的成功项。只有 envelope 为 `ok:false` 时，才表示整个请求失败。

## API 分层

### Capability 查询

第一阶段必须暴露：

```ts
function getVersion(): string;
function getCapabilities(): UbaaCapabilities;
```

这两个接口不应触发远端 I/O、本地写入或登录恢复。

### Context 与 connection mode

NAPI 内部应持有 C ABI context，并提供受控初始化/释放生命周期。可暴露有限 mode 设置：`mock`、`direct`、`vpn`/`webvpn`。非法 mode 映射为 `InvalidArgument` 或内部等价 `UbaaError`，不泄露 C 指针。

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

所有可能触发网络、文件或 secure store 的 API 均返回 `Promise<T>`。NAPI 内部不得阻塞 ArkUI 主线程；取消、超时和 session expired 必须映射为稳定错误码。

### 写操作

真实写 API 本阶段不暴露给 ArkUI。后续如暴露，必须同时满足：

- typed write service。
- ArkUI 显式二次确认。
- `writeOperations=true`。
- Core `WriteOperationGate` 通过。
- live 写专项测试已覆盖。

`writeOperations=true` 不等价于 UI 可直接真实写；`FeatureService::mutate(...)` 不作为 NAPI 真实写入口。

## Mock/offline smoke

NAPI 第一阶段 smoke 只允许覆盖：

- version。
- capability object 字段完整且为 boolean。
- mock context 下的一两个只读 API，例如 terms/todos。
- JSON envelope / `UbaaError` 映射。
- partial failure item 不被过滤。

不得在默认 smoke 中执行真实登录、真实网络、真实账号或真实写操作。

## 当前结论

本轮后可以继续 ArkUI skeleton / mock-offline 页面规划；真实只读 UI 仍需先完成并验证 C ABI + NAPI 合同。真实登录 UI 与真实写 UI 暂不进入。
