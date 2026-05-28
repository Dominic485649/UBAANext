# UBAA Next v0.5 真实协议/真实传输打磨设计

> 当前仓库版本阶段为 `v0.3.0`。本文件是 v0.5 真实协议方向的历史设计草案，不代表 v0.3 当前稳定承诺；执行前应重新对照 `docs/02-roadmap.md` 和当前代码状态。

## 背景

v0.5 的目标不是一次性完成所有校园功能，而是建立可验证、可维护、可安全运行的真实协议闭环。现有 CLI、Mock、服务抽象和部分真实协议已经具备基础，本阶段要把真实传输、认证、session、parser、业务域能力和 live smoke 验收方式统一起来。

真实账号、密码、cookie、token、ticket、验证码、session id 不属于设计文档内容，不得写入代码、文档、计划、日志、测试快照、子代理 prompt 或提交记录。真实验证只允许通过本地 `.env`、环境变量或交互输入提供凭据，并且 `.env` 必须被 Git 忽略。

## 总体边界

v0.5 采用“全面真实协议矩阵 + 分阶段 live smoke”的方式推进。

每个已存在业务域都要进入能力矩阵，明确当前状态、目标状态、传输模式、session 前置、parser 风险、错误映射、测试方式和安全要求。实现顺序按风险分层推进：先收口登录、传输、cookie、Direct/WebVPN、错误映射和脱敏规则；再补齐各业务域只读真实链路；最后处理预约、签到、提交类写操作，并为写操作增加确认门、参数校验和手动 live smoke 路径。

覆盖范围包括：

1. 认证与传输基础：WinHTTP、CookieJar、CAS/UC、SessionManager、SecureStore、WebVPN rewrite、Direct mode。
2. 教务/BYXT：课程、考试、学期、周次、成绩。
3. app.buaa 生态：SPOC、Judge、Signin、YGDK、Todo、Feature/公告。
4. 资源预约：BYKC、CGYY、LibBook。
5. 横切能力：parser 容错、业务错误识别、日志脱敏、JSON envelope、exit code、live smoke。

## 能力矩阵字段

每个业务域使用同一组字段描述：

| 字段 | 含义 |
| --- | --- |
| 业务域 | Auth、BYXT Course、Score、SPOC、Judge、Signin、YGDK、BYKC、CGYY、LibBook、Todo、Feature 等 |
| 当前状态 | Mock-only、部分真实、真实只读、真实可写、阻塞 |
| v0.5 目标 | 本阶段要达到的最低真实能力 |
| 传输模式 | Direct、WebVPN、二者都支持，或只能 Direct |
| Session 前置 | 是否需要 CAS、UC、BYXT、app.buaa、Score、CGYY 独立激活 |
| 只读操作 | 查询类命令/API，优先完成 |
| 写操作 | 预约、签到、提交、评价等，需要风险门 |
| Parser 风险 | 字段漂移、数组/对象不稳定、HTML/JSON 混合、业务错误嵌在 body 中 |
| 错误映射 | Network、Auth、Permission、Parse、RateLimit、BusinessError 等 |
| 测试方式 | 单元 mock、CLI offline、live read-only smoke、live write gated smoke |
| 安全要求 | 不落盘密码、不打印 token/cookie、日志脱敏、临时 app data |

## 通用任务模板

每个业务域拆成五步：

1. 协议确认：梳理真实 endpoint、必要 headers、cookie、redirect、mode 限制。
2. Session 闭环：确认进入该业务域前需要哪种 session 激活，失败时给出稳定错误。
3. Parser 加固：把真实响应里的字段漂移、空数据、业务错误体纳入测试。
4. 服务/CLI 契约：保证 JSON envelope、exit code、参数校验、确认门一致。
5. 验证分层：先 mock/offline，再 live read-only，最后可选 live write gate。

## 里程碑

### M1：真实基础设施收口

目标：让所有真实协议共享同一套底座。

交付内容：

- 固化 WinHTTP adapter 的 timeout、redirect、headers、cookie、TLS 和网络错误映射。
- 统一 CookieJar 与 SessionManager，避免 CAS、UC、BYXT、app.buaa、Score、CGYY 等 session 混淆。
- 明确 Direct / WebVPN mode 的职责边界。
- 完成 SecureStore/PlainFileStore 使用规则：真实 session 默认受保护存储，测试/Mock 才允许明文。
- 建立统一脱敏规则：password、token、cookie、ticket、execution、captcha、session id 不进日志、不进 JSON、不进测试快照。

验收标准：

- 登录失败、网络失败、session 过期、权限失败都有稳定 ErrorCode 和 CLI exit code。
- `--json` 输出不泄露凭据、cookie、token。
- Mock/offline 测试仍完全离线可跑。
- live 测试必须显式开启，默认不会访问真实校园系统。

### M2：教务只读真实闭环

目标：把 BYXT / Score 相关查询做成 v0.5 第一批稳定真实能力。

交付内容：

- Course、Exam、Term、Week、Grade/Score 真实查询闭环。
- 消除或隔离硬编码默认学期对真实查询的影响。
- BYXT 与 Score session 激活失败能被准确识别。
- Course/Exam/Grade parser 覆盖空数据、字段缺失、字段改名、字符串数字混用、业务错误 body。
- CLI 参数契约稳定：缺少学期、非法周次、非法课程 id 等返回参数错误，不走网络。

验收标准：

- live read-only smoke 至少覆盖登录后查当前/指定学期课程、考试、成绩、当前周次。
- 无数据场景返回空列表，不视为解析失败。
- 未登录/session 过期返回认证错误，不伪装成 parse error。
- Mock、offline CLI、live smoke 三层都有对应验证。

### M3：app.buaa 生态真实闭环

目标：让 app.buaa 相关功能不再停留在“命令存在但真实协议不完整”。

交付内容：

- 明确 app.buaa session 激活流程，并和 BYXT/Score session 解耦。
- 补齐 SPOC、Judge、Todo、Feature/公告只读链路。
- 清理核心真实路径中的 NotImplemented。
- Todo 聚合区分“全部成功为空”和“部分来源失败”。
- Signin、YGDK 区分状态查询和真实提交。
- YGDK 不在用户未明确提供图片/参数时默认提交透明图或占位图。

验收标准：

- read-only live smoke 覆盖 app.buaa session 状态、Todo、Feature/公告、SPOC/Judge 列表或状态查询。
- 写操作默认不在自动 live smoke 执行。
- 提交类命令需要显式确认或显式 `--yes`，JSON 输出不泄露敏感字段。
- 部分失败聚合结果能表达 source-level error。

### M4：预约类能力安全落地

目标：让 BYKC、CGYY、LibBook 的真实协议状态明确，并安全处理写操作。

交付内容：

- BYKC：真实 sign locations、可预约列表、预约/取消流程分离，只读优先。
- CGYY：保持 Direct-only 约束清晰，WebVPN 下直接给稳定错误；captcha/token/manual 前置条件明确。
- LibBook：座位查询、预约、取消、状态查询分层实现。
- 所有预约/取消类操作统一确认门：命令行必须展示目标、时间、地点、动作，再要求确认。
- 非幂等写操作禁止自动重试重放。

验收标准：

- live read-only smoke 可查询资源状态。
- live write smoke 只能手动启用，且每次执行前需要明确目标资源与确认。
- 取消/预约失败能区分无权限、冲突、已预约、参数错误、验证码/令牌错误、系统业务错误。
- WebVPN 不支持的域不尝试绕过或伪造流程。

### M5：live smoke 与发布门

目标：让 v0.5 可以安全验收，而不是依赖手工随意试命令。

交付内容：

- 增加独立 live smoke 入口或测试标签。
- 默认不访问真实校园系统。
- 显式环境开关启用，例如 `UBAANEXT_LIVE=1`。
- 凭据只来自本地 `.env`、环境变量或交互输入。
- `.env` 必须被 Git 忽略；仓库中只允许 `.env.example` 占位模板。
- 使用临时 `UBAANEXT_APP_DATA_DIR`，测试后不保留 session 文件。
- live smoke 分级：L1 为登录/session/read-only 查询；L2 为需要人工确认的写操作 dry-run 或预检；L3 为真实写操作，仅手动执行。
- CI 默认只跑 Mock/offline；live 只适合本地或受控机器。

验收标准：

- 没有环境开关时，live 测试全部跳过。
- live 日志不含密码、cookie、token、ticket、验证码、session id。
- 失败报告能定位到业务域和错误类型。
- v0.5 release 前至少完成 L1 read-only 验证。
- L2/L3 写操作有手动验证路径，但不要求自动化。

## 最终完成定义

v0.5 完成时，应满足：

1. 所有现有业务域都有明确真实协议状态。
2. 登录、传输、cookie、session、安全存储、错误映射成为稳定底座。
3. 教务与 app.buaa 的主要只读链路可真实验证。
4. 预约、签到、提交类写操作有确认门、风险分层和手动 live 验证路径。
5. 默认测试完全离线，live 测试显式启用且不泄露凭据。
6. Mock 能力不被破坏，仍可作为 CI 和离线开发基线。
