# 原 UBAA 后端差异报告

## 目的与结论

本文用于对齐原 UBAA（智慧北航 Remake）的后端能力语义与 UBAANext 当前 C++ core/CLI/service/test 的真实状态，服务于 HarmonyOS UI 接入前的后端判断。

当前结论：UBAANext 已经不再是 mock-only 骨架，也不能再用“是否存在 CLI 命令或 service 类”判断迁移完成度。当前项目已具备真实登录/session 基础、多个真实只读 service、parser/fixture 回归、typed 写 service、统一写门控、core 共享敏感信息脱敏，以及更完整的 CLI/live-smoke 验收入口；但多数业务域仍需要按 `ReadOnlyCandidate`、`PartiallyMigrated`、`MockOnly`、`Placeholder`、`NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified` 分层看待。

HarmonyOS UI 接真实后端前必须满足这些边界：

- 不先做真实写操作 UI。
- 不让 UI 绕过 secure store、Cookie/session 持久化、capability gate 或错误脱敏。
- 不把 `FeatureService` 的字符串 `domain/operation` routing 作为 UI 长期 API 或真实写入口。
- 不把 mock/fixture/golden happy path 当作真实后端稳定。
- 不把下游失败聚合成空列表或成功结果。
- 不默认执行 live 写操作；任何会改变远端状态的命令不得进入默认 live smoke。
- 不为了 UI 进度提前移除 curl、OpenSSL、nlohmann-json、Catch2 等当前真实协议和回归仍依赖的组件。

## 本轮新增证据

- DevEco/OpenHarmony 迁移路线已固定为双项目、单 native 真源：`D:\Code\Cpp\UBAANext` 继续作为跨平台 native SDK/Core 真源，`D:\Code\OpenHarmony\UBAANext` 专门作为 DevEco/HAP/ArkTS/ArkUI 工程壳。鸿蒙项目必须复用当前项目的 package/export target/受控源码子构建，不复制校园系统协议、parser、service、protocol、cookie/session、redaction 或 write gate。
- CMake 构建入口已拆分：host/Windows 默认继续构建 CLI，OpenHarmony preset 关闭 CLI/tests 并启用 bindings；同一 native 真源可同时服务 Windows exe/CLI 验收和 Harmony native `.so`/binding 骨架。
- 当前项目已新增最小 C ABI target `UBAANextBindingsC`，输出 `ubaanext_c`，第一批只暴露 `ubaanext_version()` 与 `ubaanext_get_capabilities(...)`。该 smoke 只证明 native package/binding 边界存在，不代表真实登录、真实只读业务或真实写完成。
- SDK/package export 骨架已补并完成 Windows package consumer smoke：当前项目安装 public headers、generated version header、platform headers、可选 C ABI headers、targets export 和 `UBAANextConfig.cmake` / `UBAANextConfigVersion.cmake`；外部消费项目可通过 `find_package(UBAANext CONFIG REQUIRED)` 链接 `UBAANext::UBAANextBindingsC`，前提是消费侧显式提供与 SDK 同 ABI/triplet 的 CURL/OpenSSL/nlohmann-json 依赖前缀。
- SPOC 单详情已补 service 合同：详情成功时保留字段和提交状态；提交信息失败时保留详情并降级为 `unknown`；登录页映射为 `SessionExpired`；业务错误消息进入共享脱敏链路。SPOC 批量详情仍无原 UBAA 语义证据，保持 `Unverified`。
- Judge `assignment details-batch` 已补 CLI JSON contract：mock/offline 下可稳定验证成功项与 `status="error"` 单项失败同时存在，`fields.submissionStatusText` 保存稳定错误码，`fields.content` 使用脱敏错误消息。
- 高敏感 diagnostics 脱敏已补共享回归：URL query、Authorization/Set-Cookie/cgAuthorization、本地路径、移动端路径、raw HTML、lock code、booking id、place/location、filename/file/path 等均不得进入错误或 diagnostics 原文。
- L1 live runner 已补只读采样入口：BYKC `profile/courses/chosen/stats`、direct-only CGYY `sites/orders/order lock-code`、LibrarySeat `libraries/reservations`、YGDK `overview/records`。这只表示 runner 支持采样；除非显式提供凭据并运行，否则不写成 live 已验证。
- Harmony UI 前置合同已明确：本轮后可开始 UI skeleton / mock-offline 页面规划；真实只读 UI 需先完成 NAPI/C API 边界合同；真实登录 UI 需 secure store、cookie persistence、`live_login` 和 session 恢复；真实写 UI 不进入。

## 状态定义

| 状态 | 含义 | UI / 后续调用方判断 |
| --- | --- | --- |
| `Aligned` | typed service、parser/fixture、CLI contract、错误语义、测试覆盖和原 UBAA 语义基本一致。 | 可作为真实只读能力接入，但仍要保留错误和空状态展示。 |
| `ReadOnlyCandidate` | 已有真实只读路径，但仍需 live smoke、字段漂移或边界样本继续证明。 | 可优先做只读 UI；不得当作写能力或完整协议承诺。 |
| `PartiallyMigrated` | 已有入口或部分真实协议，但 session、字段、业务错误、分页或状态映射不完整。 | 只能做 mock/offline、实验性只读或受限调用。 |
| `MockOnly` | 仅 mock/offline 可用；真实模式不得伪装成功。 | 只能验证 contract 和展示逻辑。 |
| `Placeholder` | 接口稳定保留，但真实语义未实现或未证明。 | 可供后续调用方适配错误展示；不能走真实成功路径。 |
| `NotImplemented` | 明确未实现，必须稳定返回 `NotImplemented`。 | 不接真实功能。 |
| `Unsupported` | 当前平台或能力不支持，必须稳定失败。 | 需要平台能力补齐后再评估。 |
| `Fallback` | 降级实现，只能作为受限路径，不得标记为完成。 | 不可替代 secure store、cookie persistence、crypto 等真实能力。 |
| `WriteGated` | 真实写操作受 typed service、显式确认和 `PlatformCapabilities::write_operations` 控制。 | 默认不做真实 UI，不进入默认 live smoke。 |
| `Unverified` | 缺少证据证明与原 UBAA 语义一致。 | 不作为真实 UI 或后续调用方承诺。 |

## 全局差异

### Native SDK / DevEco 附加结论

- 当前项目已开始从“可编译 CLI 的 native 项目”升级为“可被 DevEco 复用的 native SDK/package 输出方”：包含 `UBAANEXT_BUILD_CLI`、`UBAANEXT_BUILD_BINDINGS`、`UBAANextBindingsC`、install/export targets 和 CMake package config。
- `D:\Code\OpenHarmony\UBAANext` 只作为 DevEco/HAP/ArkTS/ArkUI 工程壳，必须复用当前项目 native package、exported target 或受控源码子构建；不得复制校园系统协议、parser、service、protocol、cookie/session、redaction 或 write gate。
- 当前 C ABI 只暴露 version/capability smoke。它是后续 NAPI/HAP 加载链路的起点，不是业务只读、真实登录或真实写完成证明。
- OpenHarmony native 构建通过只说明 core/platform/binding 可被 DevEco 消费；HAP 可构建、`.so` 可加载、NAPI smoke 通过都不改变各业务域的 `ReadOnlyCandidate`、`PartiallyMigrated`、`WriteGated`、`Unsupported` 或 `Unverified` 状态。

| 维度 | 原 UBAA 语义 | UBAANext 当前状态 | 差异与风险 |
| --- | --- | --- | --- |
| 架构 | Kotlin Multiplatform + Compose + Ktor，业务与 UI 共用原项目生态。 | C++ Core + Platform Shell；CLI 是当前主要验证入口，Harmony ArkUI 仍是后续 shell。 | UI 不能直接复制原 UBAA 调用方式，必须通过 Core typed service / NAPI 边界。 |
| CLI 验收入口 | 原 UBAA 主要通过 App 页面和共享业务代码验证能力。 | CLI 已覆盖当前主要只读、mock/offline、受控写和保留接口，并统一 JSON envelope、错误码、exit code 与脱敏策略。 | CLI 暴露只代表验收入口稳定，不代表真实协议完整；占位接口必须稳定失败。 |
| 登录与 session | App 内统一登录并维护多个下游系统会话。 | `PartiallyMigrated`：已有真实 SSO/UC 登录、session 恢复、下游激活、session expired 识别和 redaction 测试；账号、cookie、token、execution、captcha 均属敏感边界。 | Harmony 未提供真实 secure store 与 Cookie persistence 前，不应接真实登录 UI；Unsupported/Fallback/volatile store 必须 fail-closed 或显式受限。 |
| 平台能力 | 原 App 运行环境默认具备移动端存储、网络和 UI 权限。 | `PlatformCapabilities` 显式声明 `secure_store`、`cookie_persistence`、`live_login`、`upload_bytes`、`write_operations` 等能力。 | capability 缺失必须 fail-closed；`upload_bytes=true` 只代表 HTTP bytes plumbing，不代表业务上传 API 已完成。 |
| 只读协议 | 原 UBAA 已覆盖多校园系统读取。 | 多数域已有 service/parser/CLI 与离线测试，但真实字段漂移和 live smoke 仍需逐域证明。 | 不能把“有 service 类”当作协议完整；下游失败必须保留错误语义，不能聚合成空列表。 |
| 写操作 | 原 UBAA 可执行签到、预约、提交、选课等动作。 | UBAANext 写操作已收口到 typed service 和 `WriteOperationGate`，要求显式确认 + `write_operations=true`。 | `--confirm` 单独不够；默认平台能力关闭真实写，默认 live smoke 不执行真实远端写。 |
| 网络 / Cookie / Crypto / Upload | 原 App 在统一运行环境内完成 HTTP、redirect、Cookie、加密和上传。 | `PartiallyMigrated` / `Fallback` / `Unsupported` 边界已显式标注：HTTP request/response、CookieJar、CookieStore、redirect、WebVPN cipher、crypto provider、upload bytes 和共享 redaction 都可能承载敏感数据。 | 不得记录 URL query、Authorization-like header、Set-Cookie、Location、raw HTML、上传文件名、上传 bytes；UnsupportedCrypto 必须 fail-closed，不允许 plaintext fallback。 |
| 泛化功能入口 | 原 UI 可按业务页面调用具体后端。 | `FeatureService` 仅保留 mock/offline 与兼容读入口；真实写 `mutate` fail-closed。 | ArkUI 不应基于字符串 domain/operation 长期开发真实功能。 |
| 聚合待办 | 原 UBAA 聚合多来源待办。 | Todo 已支持来源级失败记录：保留成功来源，同时输出 `source-error`；来源错误消息通过 core 共享脱敏工具处理。 | UI 需要展示 partial failure，不能把失败来源显示为空待办；不得显示未脱敏下游错误。 |
| 依赖 | 原 UBAA 依赖 Kotlin/Ktor 生态。 | UBAANext 真实协议仍依赖 curl、OpenSSL、nlohmann-json、Catch2。 | 本阶段不能因 Harmony UI 迁移提前裁剪这些依赖。 |

## 业务域差异矩阵

| 功能域 | 原 UBAA 后端语义 | UBAANext 当前状态 | UI 只读接入 | 写门控状态 | Harmony 前置条件 | 主要缺口 |
| --- | --- | --- | --- | --- | --- | --- |
| 认证 / 用户会话 | 登录统一身份认证，维护用户身份和多下游 session。 | `PartiallyMigrated`：已有真实 SSO/UC 登录、session 恢复、下游激活、错误脱敏；secure store/cookie persistence 不可用时必须 fail-closed。 | 暂不建议真实 UI；可先做 mock 登录态与错误展示。 | `logout`、清缓存等本地副作用仍需确认；真实登录可能保存 cookie/session。 | secure store、Cookie persistence、live_login capability、错误脱敏和 session 恢复链路完成。 | Harmony secure store/cookie persistence 尚未证明；各下游 session 仍需 live 证据。 |
| 学期 / 周次 | 获取当前或指定学期、教学周信息。 | `ReadOnlyCandidate`：有真实只读路径和显式参数契约。 | 可优先接只读 UI。 | 无远端写。 | 复用 typed service；缺参数要展示参数错误。 | 字段漂移、空数据、live smoke 仍需持续覆盖。 |
| 课表 | 查询今日、指定日期或指定周课程。 | `ReadOnlyCandidate`：已有 service、CLI、parser 和只读契约。 | 可优先接只读 UI。 | 无远端写。 | 需要 session expired、空课表和网络错误展示。 | 节次/周次/跨周边界样本仍需扩充。 |
| 考试 | 查询考试安排。 | `ReadOnlyCandidate`。 | 可接只读 UI。 | 无远端写。 | 空考试与认证失效要区分显示。 | 字段缺失、时间地点格式漂移需更多 fixture。 |
| 空教室 | 查询校区、日期、节次下可用教室。 | `ReadOnlyCandidate`。 | 可接只读 UI。 | 无远端写。 | 校区、日期、节次参数必须由 UI 显式提供并校验。 | 校区 ID、节次编码、真实服务参数可能漂移。 |
| 成绩 | 查询当前/指定学期或全部成绩。 | `PartiallyMigrated`：已有真实只读契约，本轮补充了 Score session / 成绩请求网络错误脱敏回归，但仍属于高敏感输出。 | 可做受控只读 UI，优先 mock/offline；真实入口需谨慎。 | 无远端写。 | 必须保持 redaction，不额外记录成绩或凭据。 | 空成绩、字段漂移和 live 错误分类还需更多证据。 |
| app.buaa 版本 / 公告 | 获取 app 版本和公告。 | `ReadOnlyCandidate`。 | 可接只读 UI。 | 无远端写。 | 公告格式变化时 UI 应能显示解析失败或空状态。 | 公告格式漂移需持续 fixture。 |
| Todo 待办聚合 | 聚合 SPOC、Judge、签到、评教等待办。 | `ReadOnlyCandidate`，但必须表达 partial failure；已补充来源级错误脱敏 contract。 | 可接只读 UI，需展示 source-level error。 | 无远端写。 | UI 要保留每个来源状态，不能只看总列表是否为空。 | 仍需补更多来源的 session expired、业务错误和字段漂移样本。 |
| SPOC 作业 | 查询课程作业、待办和详情。 | `ReadOnlyCandidate` / `PartiallyMigrated`；已补充空列表、缺字段和类型漂移 parser contract；单详情 service 合同覆盖详情成功、提交状态、提交信息失败降级、session expired 和业务错误脱敏；当前没有批量详情 API。 | 可先接列表与详情只读 UI。 | 无远端写。 | 需要 pending/expired/unknown 状态展示。 | 批量详情 partial failure 仍为 `Unverified`；不要硬造无原 UBAA 证据的新批量协议。 |
| Judge / OJ 作业 | 查询作业列表、详情和批量详情。 | `ReadOnlyCandidate` / `PartiallyMigrated`；已补空课程、空作业、详情缺字段 parser contract，并补充批量详情 partial failure service 与 CLI JSON contract。 | 可先接只读 UI。 | 无远端写。 | 批量详情会保留成功项，并用 `status=error` 单项记录表达失败。 | 历史/过期过滤、字段漂移和 live 样本仍需继续补。 |
| 签到查询 | 查看今日签到或签到状态。 | `ReadOnlyCandidate`。 | 可接只读状态 UI。 | 执行签到不属于只读。 | UI 必须把“可签到”与“执行签到”分成不同动作。 | 今日列表业务失败曾有降级语义，真实错误展示需继续收口。 |
| 签到执行 | 执行课程签到。 | `WriteGated`：typed service 已接入 `WriteOperationGate`。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要风险提示、幂等/重复提交策略、位置/类型参数校验。 | live 写不可进入默认 smoke；执行结果和失败恢复需单独验证。 |
| 评教查询 | 查询评教任务和课程。 | `PartiallyMigrated`：list 失败已向上返回，Todo 可表达 evaluation source error。 | 可做实验性只读 UI。 | 提交评教不属于只读。 | UI 要区分查询失败、空任务、已评/待评。 | 问卷/课程字段漂移和 session 过期仍需更多样本。 |
| 评教提交 | 提交评价。 | `WriteGated`。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要不可逆风险提示、目标课程确认和提交结果审计。 | 提交不可逆或难撤回，禁止默认执行。 |
| 博雅课程查询 | 查询个人信息、统计、课程、已选课程和详情。 | `ReadOnlyCandidate` / `PartiallyMigrated`；已补查询 parser 空数据、字段漂移、无 id 跳过和业务错误 body 脱敏回归。 | 可先接只读 UI。 | 选课/退课/签到不属于只读。 | UI 要处理容量、状态、详情字段漂移。 | 选课接口和容量状态变化快，仍需 live smoke 候选验证。 |
| 博雅选课 / 退课 / 签到 | 改变博雅课程选择或签到状态。 | `WriteGated`。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要重复提交保护、状态刷新、失败恢复。 | 会改变真实选课/签到状态，风险高。 |
| 场馆信息 / 订单查询 | 查询场馆、用途、日期、订单和锁码。 | `ReadOnlyCandidate` / `PartiallyMigrated`；CGYY direct-only 约束需保留；已补查询 parser contract、无 id 订单跳过和业务错误 body 脱敏回归；L1 runner 已加入 direct-only `sites/orders/order lock-code` 只读采样入口。 | 可先接只读 UI。 | 预约/取消不属于只读。 | UI 不得隐式把非 direct 改路由；锁码展示要受控。 | captcha/token/sign 规则容易漂移，锁码展示仍需真实 live 样本约束。 |
| 场馆预约 / 取消 | 提交预约或取消订单。 | `WriteGated`。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要验证码、幂等、冲突处理、订单刷新策略。 | 会占用/释放真实资源，禁止默认自动执行。 |
| 图书馆座位查询 | 查询图书馆、区域、座位和当前预约。 | `ReadOnlyCandidate` / `PartiallyMigrated`；已补 parser fallback、无 id 敏感记录跳过和业务错误 body 脱敏回归。 | 可先接只读 UI。 | 预约/取消不属于只读。 | UI 要处理 token refresh、空座位、时间段选择。 | 直接/代理 URL 转换和 token 过期刷新需持续回归。 |
| 图书馆预约 / 取消 | 预约或取消座位。 | `WriteGated`。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要冲突提示、重复提交保护和预约状态刷新。 | 会改变真实座位资源状态。 |
| 阳光打卡查询 | 查询打卡概览和记录。 | `ReadOnlyCandidate` / `PartiallyMigrated`；已补分类空数据、记录字段漂移、记录业务错误脱敏和 L1 live smoke records 覆盖。 | 可接只读 UI，但隐私展示要克制。 | 提交打卡不属于只读。 | UI 需控制位置、时间等敏感信息展示。 | 记录字段和体育分类选择仍需 live 样本验证。 |
| 阳光打卡提交 / 上传 | 提交打卡记录和照片。 | `WriteGated`：已有 byte payload 抽象和 YGDK typed 写场景；`ygdk submit --photo` 会读取本地文件并可能上传图片。 | 不做默认真实 UI。 | 必须显式二次确认 + `write_operations=true`。 | 需要照片隐私提示、文件大小/类型校验、时间地点确认。 | 涉及真实记录和图片上传，风险最高之一；本地文件名和 bytes 不得泄露。 |
| 文件 / 附件上传 | 上传业务附件或图片。 | `Placeholder` / `NotImplemented`：`file upload --path <path> --confirm` 已可发现和解析，但固定返回 `NotImplemented`，不读取文件、不触发远端请求。 | 不接真实 UI；可用于后续调用方验证错误展示。 | 视具体业务按写操作处理；占位接口仍要求显式确认。 | 必须先有 typed API、fixture、隐私策略和失败语义。 | 不能把 YGDK 单场景或 CLI 占位视作全上传能力完成。 |

## CLI 验收入口当前状态

CLI 当前已经从临时调试壳推进为 core / service / parser / platform / test 的稳定验收入口，但它仍然只表达“接口和 contract 可验收”，不表达所有真实后端语义已经完成。

已更新的关键 CLI 能力：

- `grade all` 与 `grade list --all` 均作为成绩全部查询入口；真实模式会进入 session/auth gate，不再被误判为缺少 `--term` 的本地参数错误。
- `todo list [--pending-only|--all]` 会透传到 `TodoQuery`；默认仍是 pending-only，`--all` 可展示非待处理记录和来源级错误。
- `ygdk records [--page n] [--size n|--limit n]` 保留 `--limit` 兼容，内部按统一分页 size 处理；默认 L1 live smoke 已加入只读 records 覆盖。
- `file upload --path <path> --confirm` 是 `Placeholder` / `NotImplemented` 稳定占位接口：help 可发现、参数可解析、缺确认返回 `InvalidArgument`，确认后固定返回 `NotImplemented`，不会读取本地文件或触发远端请求。
- `ygdk submit --photo <path>` 是 `WriteGated` typed 写上传场景：会读取本地文件，可能上传图片并改变远端打卡状态，默认平台 `write_operations=false` 时必须失败。

CLI contract 当前固定为：

- 成功：`{"ok": true, "data": {...}, "error": null}`。
- 失败：`{"ok": false, "data": null, "error": {"code": "...", "message": "..."}}`。
- 保留接口或未实现语义使用稳定错误，不返回成功空数据。
- 错误、diagnostics 和测试输出继续走敏感信息脱敏，不得包含 username、password、cookie、token、ticket、session、captcha、authorization、上传文件名、URL query、raw HTML、本地路径、锁码、预约、打卡或座位敏感原文。

当前验证状态：

- `ubaa` 与 `UBAANextCliTests` 构建通过。
- CLI 集成测试按名称运行 39 个用例全部通过。
- 完整离线测试未出现失败输出。
- 已补 SPOC、Judge、YGDK parser contract，以及 Todo 来源级错误脱敏 contract。
- 已补 SPOC 单详情 service 合同：提交信息失败降级、session expired 和业务错误脱敏。
- 已补 BYKC、CGYY、LibrarySeat 查询 parser/service 业务错误脱敏回归。
- 已补 Grade / Score session / YGDK 高敏感只读错误脱敏回归，并增强共享 diagnostics redaction。
- 已补 Judge 批量详情 service partial failure 与 CLI JSON contract；SPOC 当前仍只有单详情 API，不硬造批量详情协议。
- L1 live runner 已覆盖计划内只读采样入口，但本报告不宣称真实 live 已运行。
- 最终构建和测试结果见本轮完成报告。

## HarmonyOS UI 接入排序

### 当前预估

- 本轮完成后可以开始 Harmony UI skeleton / mock-offline 页面规划。
- 真实只读 UI 建议在完成 NAPI/C API 边界合同后进入；该合同必须覆盖 typed service 映射、`FeatureRecord`、错误码、异步 Promise、redaction、capability 查询和 partial failure 展示。
- 真实登录 UI 必须等待 Harmony secure store、Cookie persistence、`live_login=true` 和 session 恢复链路完成。
- 真实写 UI 本阶段不进入，继续等待 typed write service、二次确认、`write_operations=true` 和 live 写专项验证。

### 第一批：可优先做只读 UI

这些页面可以先接 mock/offline，再逐步接真实只读：

1. 学期 / 周次。
2. 课表。
3. 考试。
4. 空教室。
5. app.buaa 版本 / 公告。
6. Todo 待办聚合，但必须展示各来源 partial failure。
7. SPOC / Judge 只读列表与详情。
8. 签到今日状态。
9. 博雅、场馆、图书馆、阳光打卡的查询类页面。

### 第二批：需要受控接入

这些能力可以做 UI skeleton 或 mock，但真实后端接入前需要更多证据：

- 认证 / 用户会话：等 Harmony secure store、Cookie persistence、live_login capability 和错误脱敏链路完整后再接真实登录。
- 成绩：高敏感输出，需确认 redaction、日志策略和 session 激活稳定性。
- 评教查询：可做实验性只读，但需继续补 session expired、字段漂移和业务错误样本。

### 第三批：暂不做真实写 UI

这些操作会改变远端状态，只能在后续独立专题中设计：

- 签到执行。
- 评教提交。
- 博雅选课 / 退课 / 签到。
- 场馆预约 / 取消。
- 图书馆预约 / 取消。
- 阳光打卡提交 / 图片上传。
- 任何新增附件上传或 `file upload` 的真实实现。

## 写操作安全合同

真实写操作必须同时满足：

1. 调用 typed service，不通过 `FeatureService::mutate` 字符串路由。
2. 用户在 CLI/API/UI 层完成显式确认；CLI 对应 `--confirm` 或 `--yes`，ArkUI 对应显式二次确认。
3. 当前平台 `PlatformCapabilities::write_operations == true`。
4. 默认 live smoke 不执行该操作。
5. 测试覆盖未确认、平台未授权、参数非法和业务失败路径。
6. 错误、日志、diagnostics、测试输出不泄露 username、password、cookie、token、ticket、session、captcha、authorization、上传文件名、上传 bytes、敏感 URL query、raw HTML、本地路径、锁码、预约、打卡或座位敏感原文。

任一条件缺失时必须 fail-closed。

## Todo / 聚合类只读合同

聚合类接口不能用“空列表”表达下游失败。正确语义是：

- 成功来源继续输出正常 `FeatureRecord`。
- 失败来源输出 `status = "error"` 的来源级记录。
- `fields.source` 标记来源。
- `fields.type = "source-error"`。
- `fields.errorCode` 使用稳定错误码名称。
- `fields.errorMessage` 必须经过 core 共享错误脱敏链路，不包含敏感凭据。

UI 应把 partial failure 展示为“部分来源加载失败”，而不是“没有待办”。

## 批量详情 / partial failure 合同

- Todo 等聚合类接口继续使用来源级 `source-error` 记录，保留成功来源，不把失败吞成空列表。
- Judge 批量详情现在保留成功详情，并为单项失败返回 `status = "error"` 的 `JudgeAssignmentDetail` / `FeatureRecord`；`status_text` / `fields.submissionStatusText` 保存稳定错误码，`content` / `fields.content` 保存脱敏后的错误消息。
- SPOC 当前只有单详情 service/CLI，没有批量详情 API；批量 partial failure 仍为 `Unverified`，不得为了 UI 进度硬造无原 UBAA 证据的新批量协议。

## 依赖冻结结论

当前后端真实能力仍依赖以下组件，本阶段不应删除或替换：

| 依赖 | 当前作用 |
| --- | --- |
| curl | 真实 HTTP、redirect、TLS、Cookie/session、WebVPN/direct 请求路径。 |
| OpenSSL | AES/MD5/RSA/sign 等真实协议加密能力。 |
| nlohmann-json | service/parser/CLI JSON 输出和 fixture/golden 回归。 |
| Catch2 | 单元测试、集成测试、协议契约和回归保护。 |

只有在真实只读协议稳定、替代方案经过测试和文档证明后，才能把依赖裁剪作为单独议题评估。

## 后续验收清单

HarmonyOS UI 接真实后端前，至少完成：

- Harmony 平台 secure store 可用，且不退回 Unsupported/Volatile/明文实现。
- Cookie persistence 可用，并与 live CookieJar 生命周期一致。
- `live_login` capability 与真实登录入口绑定。
- `write_operations` 默认关闭，真实写 UI 不在第一批接入。
- `file upload` 占位仍稳定返回 `NotImplemented`；任何真实上传必须先落到 typed API 并通过本地文件读取、隐私、失败语义和写门控测试。
- 只读 UI 仅调用 typed service 或明确的只读兼容 API。
- Todo 和其他聚合 UI 展示 partial failure。
- 默认 live smoke 只包含只读命令。
- 错误、日志、diagnostics、测试输出保持敏感信息脱敏。
- CLI contract 与对应 mock/offline 测试稳定通过；保留接口必须稳定失败，不伪装成功。
- 每个 UI 页面有对应 offline fixture/parser/service 测试锚点。
- DevEco 项目通过当前项目 native package/export target/受控源码子构建复用，不复制 core/service/parser/protocol。
- C ABI/NAPI 第一阶段只做 version/capability/mock-offline smoke，不把 `.so` 可加载当作真实业务完成。
- HAP 不打包 CLI exe、tests、fixtures、凭据、本地敏感配置或开发机路径。

下一批建议：

1. 在 `D:\Code\OpenHarmony\UBAANext` 建 DevEco 工程壳，只接入当前项目 native package/binding，不复制业务协议；第一批仅验证 HAP packaging、`.so` 加载、version/capability smoke。
2. 为 OHOS ABI 固化依赖 manifest：commit sha、version、compiler、CURL/OpenSSL/nlohmann-json 来源、license notices、capability 状态和 SDK 同源依赖前缀。
3. 完成 OpenHarmony native 构建验证：设置 `OHOS_NATIVE_HOME`、`UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR` 和 OHOS ABI 下 CURL/OpenSSL 来源后运行 `openharmony-clang-*` preset；若依赖缺失，保持未验证状态。
4. 完成 C ABI/NAPI 边界合同的下一层实现和测试：capability 查询、typed readonly service 映射、错误码映射、Promise 异步边界、redaction 和 partial failure 展示。
5. 基于当前文档开始 Harmony UI skeleton / mock-offline 页面规划，但真实只读 UI 仍需等待 C ABI + NAPI 合同验证。
6. 为 SPOC 单详情补更多真实样本 fixture，继续明确详情字段、提交状态和 session expired 的稳定映射；批量详情继续保持 `Unverified`，直到有原 UBAA 语义证据。
7. 在用户显式授权并提供凭据时，分 direct/WebVPN 运行 L1 live 只读采样，记录 BYKC、CGYY、LibrarySeat、YGDK 的真实可用性；写操作继续不进入默认 smoke。
8. 为成绩、场馆锁码、预约记录、座位预约和打卡记录补更多高敏感输出的日志/diagnostics 禁止回归，尤其避免固化真实业务明细。
9. 为 Judge 历史/过期过滤补更多 CLI JSON contract 和 live 样本，继续验证 `status=error` 单项记录不会被后续上层客户端误判为空结果。
10. Harmony 真实登录 UI 前先完成 secure store、Cookie persistence、`live_login` capability 与 session 恢复链路；真实写 UI 继续等待独立写操作安全专题。
