# UBAA Next v0.5 真实协议总览与分卷设计

> 当前仓库版本阶段为 `v0.3.0`。本文件是 v0.5 真实协议方向的历史设计草案，不代表 v0.3 当前稳定承诺；执行前应重新对照 `docs/02-roadmap.md` 和当前代码状态。

## 背景

v0.5 的目标不是一次性补齐所有校园功能，而是建立一套可持续推进的真实协议体系：真实登录、真实 session、真实只读查询、受控写操作、安全 live smoke、稳定错误契约和可维护测试策略。

当前代码已经有部分真实协议能力，也有 Mock/offline 基线。但真实协议分散在多个 service 中，session、错误处理、脱敏、CLI 参数传递、live smoke 和测试覆盖尚未完全统一。因此 v0.5 需要先定义分卷边界，再逐卷设计和实施。

本设计只覆盖 UI 以外的能力。

## 目标

v0.5 总目标：让 UBAA Next 的真实协议能力从“零散可用”进入“分域明确、安全可验收、可持续扩展”的状态。

具体目标：

1. 每个业务域都有明确当前状态、v0.5 目标、传输模式、session 前置、风险和验收方式。
2. 登录、session、cookie、SecureStore、Direct/WebVPN、脱敏和错误模型成为真实协议底座。
3. BYXT / Score 的核心只读能力优先稳定：学期、周次、课表、考试、成绩。
4. app.buaa 生态先完成只读链路：公告、Todo、SPOC、Judge、签到状态、YGDK overview / records。
5. 预约、签到、提交、评教等有副作用操作后置，并受强确认门保护。
6. 默认测试完全离线；真实验证只通过显式 live smoke 执行。
7. 真实账号、密码、cookie、token、ticket、验证码、session id 不出现在代码、文档、日志、测试快照或提交记录中。

## 非目标

v0.5 总览设计不做以下事情：

1. 不设计 UI、ArkUI 页面或桌面界面。
2. 不在本总览中写每个接口的完整请求参数和响应字段。
3. 不要求一次性实现所有原 UBAA 功能。
4. 不把真实写操作纳入默认自动测试。
5. 不把真实凭据或真实 session 样本写入仓库。

接口级细节放到后续分卷设计中。

## 分卷结构

v0.5 拆为六卷。第 0 卷是主控蓝图，后续每卷都可以独立写 spec、plan 和实现。

### 第 0 卷：总览与分卷边界

本卷定义：

- v0.5 总目标；
- 分卷边界；
- 能力矩阵字段；
- 优先级；
- 跨卷硬规则；
- 总体验收门槛。

本卷不深入具体接口。

### 第 1 卷：登录 / session / 安全底座

覆盖：

- CAS / UC 登录；
- CookieJar；
- SecureStore；
- session 文件；
- Direct / WebVPN URL rewrite；
- 业务域 session 分离；
- redaction helper；
- 统一错误码；
- 敏感输出测试。

这是所有真实协议的地基，优先级最高。

### 第 2 卷：BYXT / Score 只读闭环

覆盖：

- 学期；
- 周次；
- 课表；
- 考试；
- 成绩；
- 当前学期解析；
- CLI `--term` 行为；
- BYXT session；
- Score session；
- 字段漂移、空数据和业务错误 body；
- 只读 live L1 smoke。

这卷是 v0.5 第一批稳定真实查询能力。

### 第 3 卷：app.buaa 生态

覆盖：

- app.buaa session；
- 公告；
- Todo；
- SPOC；
- Judge；
- Signin 状态；
- YGDK overview / records；
- 只读和轻写操作边界。

本卷先做只读，提交类操作延后到第 4 卷。

### 第 4 卷：预约与提交类操作

覆盖：

- BYKC 选课 / 退选 / 签到；
- CGYY 场馆预约 / 取消；
- LibBook 座位预约 / 取消；
- Signin do；
- YGDK submit；
- Evaluation submit；
- 强确认门；
- live write gate；
- 非幂等请求禁止自动重试。

这是风险最高的一卷，必须在底座和只读能力稳定后实施。

### 第 5 卷：live smoke 与发布门

覆盖：

- `.env.example`；
- 默认离线；
- L1 / L2 / L3 分级；
- 临时 app data；
- 日志脱敏；
- CI 与本地验证边界；
- release gate。

这卷编号靠后，但基础能力应尽早落地，因为每个真实功能都需要统一验证入口。

## 架构边界

v0.5 按层划分职责。

### CLI 层

职责：

- 参数解析；
- 参数校验；
- 确认门；
- JSON / 文本输出；
- exit code 映射；
- 将通用参数传入 service，例如 `--term`、`--mode`、分页、ID。

CLI 不直接拼真实接口 URL，也不直接处理 session。

### Service 层

职责：

- 表达业务动作；
- 调用对应 protocol/session helper；
- 调用 parser；
- 决定只读数据是否缓存；
- 返回稳定 `Result<T>`。

Service 不重复实现 cookie、登录页识别、脱敏或 URL rewrite。

### Protocol / Session 层

职责：

- 真实系统 session 激活；
- 默认 headers；
- Direct / WebVPN URL rewrite；
- redirect 判断；
- 登录失效识别；
- 业务系统错误初步识别。

每个真实系统应有独立边界，例如 BYXT、Score、app.buaa、CGYY、YGDK。

### Net / Storage / Security 底座

职责：

- HTTP 请求；
- redirect；
- cookie；
- proxy；
- TLS；
- timeout；
- SecureStore；
- CacheStore；
- redaction。

底座不理解业务语义。

### Parser 层

职责：

- 将响应结构转成 model；
- 兼容字段缺失、字段漂移、数字/字符串混用；
- 区分空数据和解析失败。

Parser 不负责登录流程。

## 标准数据流

真实协议的标准数据流为：

```text
CLI 参数
  -> Service 业务调用
  -> Protocol/session helper
  -> HTTP client
  -> Parser
  -> Model
  -> Result
  -> CLI 输出
```

排查原则：

- 参数错看 CLI；
- session 错看 Protocol；
- 网络错看 Net；
- 字段错看 Parser；
- 业务语义错看 Service；
- 输出和 exit code 错看 CLI formatter。

## 能力矩阵字段

后续每卷都应维护或引用同一套能力矩阵字段：

| 字段 | 含义 |
| --- | --- |
| 业务域 | Auth、BYXT Course、Score Grade、Feature 公告、YGDK 等 |
| 当前状态 | Mock-only、部分真实、真实只读、真实可写、阻塞 |
| v0.5 目标 | 本阶段最低交付目标 |
| 传输模式 | Direct、WebVPN、二者都支持、仅 Direct |
| Session 前置 | CAS、UC、BYXT、Score、app.buaa、CGYY、无 |
| 只读操作 | list、show、overview、records 等 |
| 写操作 | submit、reserve、cancel、sign 等 |
| 风险点 | 字段漂移、验证码、重复提交、敏感信息、WebVPN 不兼容等 |
| 验收方式 | unit mock、CLI offline、live L1、live L2/L3 |
| 所属分卷 | 底座、BYXT/Score、app.buaa、预约写操作、live smoke |

总览只定义字段。具体矩阵内容在后续分卷补全。

## 跨卷硬规则

### 默认离线

默认构建、默认单元测试、默认 CLI integration 都不能访问真实校园系统。

只有显式设置 live 环境开关，例如 `UBAANEXT_LIVE=1`，才允许访问真实校园系统。

### 敏感信息保护

真实账号、密码、cookie、token、ticket、验证码、session id：

- 不写进代码；
- 不写进文档；
- 不写进测试 fixture；
- 不写进日志；
- 不进入 JSON 输出；
- 不进入提交记录；
- 不传给子代理 prompt；
- 不出现在失败消息中。

任何命令回显、HTTP 调试输出或错误文本都必须先脱敏。

### Session 分域

不同系统的 session 必须明确分域：

- CAS / UC；
- BYXT；
- Score；
- app.buaa；
- CGYY；
- YGDK；
- 其他独立业务域。

不能假设一个系统登录成功就代表所有业务域都可用。

### 只读优先

真实能力优先级：

1. 登录和 session；
2. 学期、周次、课表、考试、成绩；
3. 公告、Todo、SPOC、Judge、签到状态、YGDK 状态；
4. 预约、取消、提交、签到、评教等写操作。

### 写操作强确认

写操作至少需要两层保护：

1. CLI 层确认门，例如 `--confirm` 或 `--yes`；
2. live smoke 层环境开关，例如 `UBAANEXT_ALLOW_WRITE=1`。

真实写操作还应有更强开关，例如 `UBAANEXT_CONFIRM_WRITE=1`。

自动 live L1 永远只做只读。

### 错误语义稳定

错误不能全部混成 NetworkError 或 ParseError。至少要区分：

- 参数错误；
- 未登录 / session 过期；
- 权限不足；
- 业务拒绝；
- 网络失败；
- 解析失败；
- 功能未实现。

CLI exit code 和 JSON envelope 必须稳定。

### Parser 稳定性

真实响应 parser 必须覆盖：

- 空数组；
- 空对象；
- 字段缺失；
- 字段改名；
- 数字/字符串混用；
- 业务错误 JSON；
- HTML 登录页误返回；
- 部分成功 / 部分失败。

没有真实样本的 parser 只能标为假设性支持，不能标为已完成。

## 测试策略

### Unit mock 测试

要求：

- 不访问真实网络；
- 使用最小响应样本；
- 覆盖成功、空数据、业务错误、session 过期、字段漂移。

### CLI offline 测试

要求：

- 验证参数解析；
- 验证 exit code；
- 验证 JSON envelope；
- 验证 `--term`、`--confirm`、`--mode` 是否真正生效；
- 验证缺参数时不会发起网络请求。

### Live L1 只读 smoke

要求：

- 只在本地显式启用；
- 使用真实账号；
- 只做登录、session、只读查询；
- 使用临时 app data；
- 日志必须脱敏。

### Live L2 / L3 写操作验证

要求：

- 默认不运行；
- 只适合人工执行；
- 执行前展示目标资源、动作、时间、地点；
- L3 真实写操作必须有额外确认开关；
- 非幂等请求失败后不自动重试。

## 交付顺序

推荐顺序：

1. 第 1 卷：登录 / session / 安全底座。
2. 第 2 卷：BYXT / Score 只读闭环。
3. 第 5 卷：live smoke 与发布门的基础版本。
4. 第 3 卷：app.buaa 生态。
5. 第 4 卷：预约与提交类操作。

理由：先保证安全和 session，再保证核心只读，再建立统一验证入口，最后处理真实写操作。

## 总体验收门槛

v0.5 总览完成后，后续每卷进入实现前必须满足：

1. 明确所属业务域和边界。
2. 明确依赖的 session 类型。
3. 明确 Direct / WebVPN 支持状态。
4. 明确只读和写操作边界。
5. 明确敏感信息处理规则。
6. 明确错误码和 CLI exit code 期望。
7. 明确 unit mock、CLI offline、live smoke 验证方式。
8. 明确哪些能力没有真实样本支撑。

v0.5 最终完成时，应满足：

1. 所有现有业务域都有明确真实协议状态。
2. 登录、传输、cookie、session、安全存储、错误映射成为稳定底座。
3. 教务与 app.buaa 主要只读链路可真实验证。
4. 预约、签到、提交类写操作有确认门和风险分层。
5. 默认测试完全离线。
6. live 测试显式启用且不泄露凭据。
7. Mock 能力不被破坏。

## 后续设计顺序

本总览通过后，下一份 spec 应为：

1. `v05-auth-session-security-design`：登录 / session / 安全底座。
2. `v05-byxt-score-readonly-design`：BYXT / Score 只读闭环。
3. `v05-live-smoke-release-gates-design`：live smoke 与发布门基础版。
4. `v05-app-buaa-readonly-design`：app.buaa 生态只读能力。
5. `v05-write-operations-design`：预约与提交类操作。
