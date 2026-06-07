# NAPI API 计划

> 当前仓库版本阶段为 `v0.4.0`。NAPI 属于路线图 `v0.6 — HarmonyOS NAPI` 后续计划，本页是边界设计草案，不代表 v0.4 当前稳定 API。

## 目标

NAPI 层是 Harmony ArkUI 调用 UBAANext Core 的边界。它不重新实现校园系统协议，不直接处理 cookie/session/token 持久化，也不绕过 Core capability gate、write gate 和错误脱敏。真实只读 UI 进入前，必须先冻结本文件中的 API 合同并通过 C ABI / NAPI smoke。

当前项目 `D:\Code\Cpp\UBAANext` 是 native 真源；DevEco 项目 `D:\Code\OpenHarmony\UBAANext` 只负责鸿蒙工程、HAP、Ability 和 ArkTS/ArkUI。NAPI wrapper 应复用当前项目输出的 C ABI/native SDK/package，不复制 core/service/parser/protocol。

## 当前 native 基线

当前 C ABI 已不只是 version/capability 骨架，`UBAANextBindingsC` / `ubaanext_c` 已包含：

- version 与 capability，其中 JSON envelope 版本使用 camelCase capability 字段供 ArkTS 直接映射。
- context 生命周期和 connection mode。
- mock/offline login、whoami/session state 与 logout 基础。
- 学期、周次、课表、成绩、考试、Todo、签到今日、YGDK 概览/记录等只读实验接口。
- 课堂资源只读实验接口：`liveWeek`、`liveResources`、`liveDetail`，对应 C ABI 的 `ubaanext_live_week/resources/detail`，用于 Harmony 侧发现课堂直播/回放/PPT GUID 与视频候选。
- FeatureRecord 通用只读投影接口，复用 Core `FeatureService` 路由到 evaluation、SPOC、Judge、BYKC、CGYY、LibBook 等已迁移 typed service；NAPI 可先将其作为只读列表/详情桥接，不得把它扩展成写通道。
- TD 本地投影：status、user list、count cache，以及带显式确认的本地图片删除 `tdImageDelete`；这些接口不刷新远端计数、不上传图片、不执行打卡。
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
function getVersionInfo(): UbaaEnvelope<{ version: string }>;
function getCapabilities(): UbaaCapabilities;
```

这三个接口不应触发远端 I/O、本地写入或登录恢复。`getCapabilities()` 可由 C ABI struct 或 `ubaanext_capabilities()` envelope 映射；ArkTS 不解析 CLI 文本。

### Context 与 connection mode

NAPI 内部应持有 C ABI context，并提供受控初始化/释放生命周期。可暴露有限 mode 设置：`mock`、`direct`、`vpn`/`webvpn`。非法 mode 映射为 `InvalidArgument` 或内部等价 `UbaaError`，不泄露 C 指针。

### 登录与会话

第一阶段只把会话合同打通到 native 边界：

```ts
interface SessionInfo {
  active: boolean;
  mode: 'mock' | 'direct' | 'webvpn';
  account: { studentId: string; displayName: string } | null;
}

function login(username: string, password: string, captcha?: string): Promise<{ account: SessionInfo['account'] }>;
function whoami(): Promise<SessionInfo>;
function getSessionState(): Promise<SessionInfo>;
function restoreSession(): Promise<{ active: true; account: SessionInfo['account'] }>;
function logout(): Promise<{ active: false }>;
```

Mock mode 的 `login` 只验证 native 合同和本地 session 投影，不证明真实 CAS 语义。非 mock mode 的真实登录必须 fail-closed：平台缺少 `liveLogin` 或 `cookiePersistence` 时返回 `UnsupportedPlatform`，不得让 Harmony 缺失安全/持久化存储时假装保存成功。

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
- Classroom live/resources/detail，用于课堂资源发现、状态过滤、与课表合并，以及 detail 中的 PPT GUID/视频 URL/HLS 标记展示。下载落盘、PPTX 生成、ffmpeg 合并仍暂不作为 Harmony NAPI 默认接口。
- TD status/users/count cache 本地只读投影。

在 typed wrapper 逐项稳定前，NAPI 可先暴露只读 FeatureRecord 通用投影：

```ts
interface FeatureQuery {
  domain: 'evaluation' | 'spoc' | 'judge' | 'bykc' | 'cgyy' | 'libbook' | 'signin' | 'ygdk' | 'announcement' | 'app';
  operation: string;
}

function listFeatures(query: FeatureQuery): Promise<FeatureRecord[]>;
function showFeature(query: FeatureQuery & { id?: string }): Promise<FeatureRecord>;
```

`listFeatures`/`showFeature` 仅映射 C ABI `ubaanext_feature_list(...)` 与 `ubaanext_feature_show(...)` 的只读投影。它适合让 Harmony MVP 消费已经迁移但尚未逐个 typed wrapper 化的服务；后续 typed API 稳定后可以逐步替换为具体函数。NAPI 不得通过该接口调用 `FeatureService::mutate(...)` 或真实写操作。

所有可能触发网络、文件或 secure store 的 API 均返回 `Promise<T>`。NAPI 内部不得阻塞 ArkUI 主线程；取消、超时和 session expired 必须映射为稳定错误码。

### 课堂资源 native wrapper

当前 OpenHarmony NAPI wrapper 已提供同步 JSON envelope 字符串入口，后续 ArkTS 服务层可包一层 Promise/DTO 解析：

```ts
interface LiveWeekQuery { startDate: string; endDate: string; }
interface LiveResourcesQuery {
  date: string;
  status?: 'live' | 'playback' | 'generating' | 'all';
  fromCourse?: boolean;
}
interface LiveDetailQuery { courseId: string; subId: string; date?: string; }

function liveWeek(query: LiveWeekQuery): string | UbaaError;
function liveResources(query: LiveResourcesQuery): string | UbaaError;
function liveDetail(query: LiveDetailQuery): string | UbaaError;
```

`liveResources(..., fromCourse:true)` 会在 C++ core 内复用当天教务课表做过滤；ArkTS 不应重新实现课堂资源搜索协议，也不应直接访问 classroom/yjapi/livingroom。

### TD 本地图片删除

本地 TD 图片删除是文件系统 mutation，不触发真实 TD 服务器请求。NAPI wrapper 只提供显式确认入口：

```ts
interface TdImageDeleteRequest {
  name: string;
  force?: boolean;
  confirmed: boolean;
}

function tdImageDelete(request: TdImageDeleteRequest): string | UbaaError;
```

`confirmed` 必须为 `true`；`force` 仅用于允许删除仍被 TD 用户引用的本地图片，不绕过安全文件名校验。ArkUI 必须在 UI 层提供二次确认和引用风险说明。

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
- mock context 下的 login/whoami/session state。
- mock context 下的一两个只读 API，例如 terms/todos。
- mock context 下通过 `listFeatures(...)` 覆盖 evaluation、SPOC、Judge、BYKC、CGYY、LibBook 的 FeatureRecord envelope。
- TD 本地只读投影在空 store 下返回空数组 envelope。
- JSON envelope / `UbaaError` 映射。
- partial failure item 不被过滤。

不得在默认 smoke 中执行真实登录、真实网络、真实账号或真实写操作。

## 当前结论

本轮后可以继续 ArkUI skeleton / mock-offline 页面规划；真实只读 UI 仍需先完成并验证 C ABI + NAPI 合同。真实登录 UI 与真实写 UI 暂不进入。
