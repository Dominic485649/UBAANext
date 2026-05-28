# UBAA Next v0.5 真实协议/真实传输实施计划

> 当前仓库版本阶段为 `v0.3.0`。本文件是 v0.5 真实协议方向的历史实施草案，不代表 v0.3 当前稳定承诺；执行前应重新对照 `docs/02-roadmap.md` 和当前代码状态。

## 约束

- 不读取、不打印、不记录根目录 `.env` 的真实内容。
- 真实账号、密码、cookie、token、ticket、验证码、session id 不写入代码、文档、计划、日志、测试快照、子代理 prompt 或提交记录。
- `.env` 只作为本地 live smoke 输入源，仓库只保留 `.env.example` 占位模板。
- 默认测试必须完全离线；只有显式启用 `UBAANEXT_LIVE=1` 才允许访问真实校园系统。
- 预约、签到、提交、评价等写操作默认禁止自动执行，必须有显式风险门。
- 不破坏现有 Mock/offline 测试和 CLI JSON/exit code 契约。

## P0：准备与基线

### P0.1 建立本地凭据与忽略规则

修改点：

- 根目录 `.gitignore`：确认包含 `.env`。
- 根目录 `.env.example`：新增或更新占位字段，不包含真实值。

建议字段：

```dotenv
UBAANEXT_LIVE=0
UBAANEXT_USERNAME=
UBAANEXT_PASSWORD=
UBAANEXT_APP_DATA_DIR=
UBAANEXT_CONNECTION_MODE=direct
UBAANEXT_LIVE_LEVEL=L1
UBAANEXT_ALLOW_WRITE=0
UBAANEXT_CONFIRM_WRITE=0
```

验收：

- `.env` 不在 Git 状态中显示为待提交文件。
- `.env.example` 只包含空值或安全默认值。

### P0.2 固定离线测试基线

修改点：

- 不改业务代码前先跑当前相关测试，作为 v0.5 基线。
- 优先跑局部测试；全量测试只在里程碑完成或发布门前运行。

验收：

- 当前 Mock/offline 单元测试通过。
- 当前 CLI integration 测试通过。

## M1：真实基础设施收口

目标：所有真实协议共享一致的传输、session、存储、错误和脱敏底座。

### M1.1 WinHTTP 行为契约

重点文件：

- `core/include/UBAANext/Net/HttpClient.hpp`
- WinHTTP adapter 实现文件
- 相关 HTTP/Cookie 测试文件

任务：

1. 明确 timeout、redirect、headers、cookie、TLS、proxy 的行为契约。
2. 确认网络层错误统一映射为稳定 `ErrorCode`。
3. 对 redirect 深度、空响应、非 2xx、TLS/连接失败增加测试。
4. 确认非幂等请求不被自动重试重放。

验收：

- HTTP adapter 测试覆盖成功、超时、redirect、错误状态码、cookie 保存。
- JSON 输出和错误文本不含敏感 header 或 cookie。

### M1.2 SessionManager / CookieJar 分域隔离

重点文件：

- session/cookie 相关 core 模块
- `core/src/Auth/AuthService.cpp`
- BYXT、app.buaa、Score、CGYY protocol/session helper

任务：

1. 梳理 CAS、UC、BYXT、app.buaa、Score、CGYY 的 session key 和生命周期。
2. 确认不同业务域 session 不互相覆盖。
3. 统一 session 过期、未登录、激活失败的错误类型。
4. 增加 session 过期与重新激活测试。

验收：

- 业务域 session 激活失败时返回 Auth/Permission/BusinessError 中的稳定一种，不落入 Parse。
- Cookie 持久化与清理行为可测试。

### M1.3 SecureStore 与脱敏规则

重点文件：

- `apps/cli/include/PlainFileStore.hpp`
- `apps/cli/src/PlainFileStore.cpp`
- CLI 输出、错误格式、JSON envelope 相关模块

任务：

1. 确认 Windows 真实 session 默认使用受保护存储。
2. 明文存储只用于测试/Mock/offline 场景。
3. 建立统一 redaction helper 或集中规则。
4. 覆盖 password、cookie、token、ticket、execution、captcha、session id 的脱敏测试。

验收：

- CLI 成功和失败输出均不泄露敏感值。
- 测试快照中不出现敏感字段值。

## M2：BYXT / Score 只读真实闭环

目标：课程、考试、学期、周次、成绩成为第一批稳定真实查询能力。

### M2.1 Term / Week 真实来源收口

重点文件：

- Term/Week service 与 parser
- Course/Exam/Grade service 中使用默认学期的位置

任务：

1. 找出真实查询里硬编码或 fallback 默认学期。
2. 优先使用 term API 或显式用户参数。
3. 参数缺失时返回参数错误，避免静默使用错误学期。
4. 增加 term/week parser 的空数据与字段漂移测试。

验收：

- 无显式学期且无法从真实 API 获取时，不发起错误查询。
- 当前周次和学期查询可 live read-only 验证。

### M2.2 Course / Exam 查询闭环

重点文件：

- `core/src/Service/CourseService.cpp`
- Exam service/parser 文件
- CLI course/exam 命令处理文件

任务：

1. 确认 BYXT session 激活失败的错误映射。
2. 加固课程和考试 parser：空数组、字段缺失、字符串数字混用、业务错误 body。
3. 确认 CLI 参数校验在发起网络前完成。
4. 增加 mock/offline 测试和 live L1 smoke 覆盖。

验收：

- 课程/考试无数据返回空列表。
- 未登录或 BYXT 激活失败返回认证/session 类错误。
- live L1 可查询课程和考试。

### M2.3 Grade / Score 查询闭环

重点文件：

- Grade/Score service 与 protocol helper
- Score session 激活模块
- Grade parser 测试

任务：

1. 整理 Score session redirect 深度与失败页识别。
2. 加固成绩 parser：空成绩、补考/重修字段、字段缺失、业务错误 body。
3. 区分未授权、无成绩、解析失败。
4. 增加 live L1 成绩查询 smoke。

验收：

- 无成绩和解析失败可区分。
- Score session 激活失败不会伪装成 parse error。

## M3：app.buaa 生态真实闭环

目标：SPOC、Judge、Signin、YGDK、Todo、Feature/公告有明确真实协议状态，并优先完成只读链路。

### M3.1 app.buaa session 基础

重点文件：

- app.buaa protocol/session helper
- SPOC/Judge/Signin/YGDK/Todo/Feature service

任务：

1. 梳理 app.buaa session 激活流程。
2. 与 BYXT/Score session 明确解耦。
3. 统一 app.buaa 未登录、权限失败、业务拒绝错误映射。
4. 增加 session 激活和失败测试。

验收：

- app.buaa session 状态可 live L1 验证。
- app.buaa session 错误不污染 BYXT/Score 状态。

### M3.2 Feature / 公告真实协议补齐

重点文件：

- `core/src/Service/FeatureService.cpp`
- Feature parser/test
- CLI feature/announcement 相关命令

任务：

1. 补齐公告/Feature 的真实只读 endpoint。
2. 清理核心真实路径中的 `NotImplemented`。
3. 支持列表、详情、空列表和业务错误 body。
4. 增加 mock/offline 与 live L1 smoke。

验收：

- 公告/Feature 不再在真实只读路径返回 `NotImplemented`。
- 空公告返回空列表，协议失败返回业务或网络错误。

### M3.3 Todo 聚合错误表达

重点文件：

- Todo service/parser
- Todo CLI JSON 输出测试

任务：

1. 区分“所有来源成功但为空”和“部分来源失败”。
2. JSON 输出表达 source-level error，且不破坏现有成功结构。
3. 对部分失败、全部失败、全部为空增加测试。

验收：

- Todo live L1 可以显示成功来源和失败来源。
- 部分失败不被静默吞掉。

### M3.4 SPOC / Judge / Signin / YGDK 分层

重点文件：

- SPOC、Judge、Signin、YGDK service/parser/CLI

任务：

1. SPOC/Judge 先完成列表、状态、详情等只读能力。
2. Signin/YGDK 区分状态查询和真实提交。
3. YGDK 禁止在缺少用户明确图片/参数时提交默认透明图或占位图。
4. 所有提交类命令接入统一确认门。

验收：

- live L1 只执行查询，不执行提交。
- 真实提交必须要求 `UBAANEXT_ALLOW_WRITE=1` 和交互确认或显式确认参数。

## M4：BYKC / CGYY / LibBook 预约类能力

目标：预约类协议优先完成只读和状态查询，写操作带强确认门。

### M4.1 BYKC 查询与预约分离

重点文件：

- BYKC service/parser/protocol
- BYKC CLI 命令

任务：

1. 生成真实 sign locations 和可预约资源查询。
2. 预约、取消、签到与查询逻辑分离。
3. 写操作展示目标资源、时间、地点、动作。
4. 非幂等请求禁止自动重试。

验收：

- live L1 只查询资源状态。
- live L3 手动写操作必须明确确认。

### M4.2 CGYY Direct-only 规则

重点文件：

- CGYY / VenueReservation service/protocol/CLI

任务：

1. 固化 CGYY 必须 Direct 的错误契约。
2. WebVPN 下不尝试绕过，直接返回稳定错误。
3. 明确 captcha/token/manual 前置条件。
4. 查询与预约/取消分层。

验收：

- WebVPN 下 CGYY 返回可预期错误。
- Direct 查询可 live L1 验证。

### M4.3 LibBook 查询与写操作风险门

重点文件：

- LibBook / LibrarySeat service/parser/CLI

任务：

1. 完成座位查询、预约状态查询。
2. 预约和取消接入统一确认门。
3. 区分无权限、冲突、已预约、参数错误、系统拒绝。

验收：

- live L1 可查询座位/预约状态。
- live L3 写操作只能手动启用。

## M5：live smoke 与发布门

目标：提供安全、可重复、默认关闭的真实协议验收路径。

### M5.1 `.env.example` 与本地环境加载

重点文件：

- `.env.example`
- `.gitignore`
- live smoke runner 或 CLI 测试辅助代码

任务：

1. 新增 `.env.example` 安全模板。
2. 确认 `.env` 已忽略。
3. live smoke 入口支持从环境变量读取配置。
4. 如需要读取 `.env`，只在本地测试入口加载，不在业务 core 中耦合 dotenv。

验收：

- 未设置 `UBAANEXT_LIVE=1` 时全部 live smoke 跳过。
- `.env.example` 不含真实值。

### M5.2 live smoke 分级

任务：

1. L1：登录/session/read-only 查询。
2. L2：写操作 dry-run 或预检，不真实提交。
3. L3：真实写操作，只允许手动运行。
4. 失败输出包含业务域、阶段、错误类型，不包含敏感值。

验收：

- 默认 `ctest` 不访问真实网络。
- 显式 live L1 可运行只读 smoke。
- L2/L3 不会被 CI 或默认测试误触。

### M5.3 发布前验收顺序

发布前顺序：

1. 跑相关单元测试。
2. 跑 CLI offline integration。
3. 跑全量 Mock/offline 测试。
4. 本地启用 live L1，验证登录、session、课程、考试、成绩、Todo、公告、资源查询。
5. 对需要真实写操作的域，人工选择 L2/L3 验证，不作为默认发布阻塞项。

v0.5 release gate：

- M1 必须完成。
- M2 必须完成。
- M3 至少完成 app.buaa session、Todo、Feature/公告和主要只读查询。
- M4 至少完成只读查询与写操作风险门。
- M5 必须完成 L1 live smoke 和默认离线安全门。

## 推荐实施顺序

1. 提交 `.env.example` 和 `.gitignore` 安全基线。
2. 加 live smoke 配置读取与默认 skip 机制。
3. 收口脱敏 helper 和敏感输出测试。
4. 收口 session/cookie 错误映射。
5. 完成 BYXT Term/Week/Course/Exam。
6. 完成 Score/Grade。
7. 完成 app.buaa session、Feature/公告、Todo。
8. 完成 SPOC/Judge 只读。
9. 收紧 Signin/YGDK 写操作风险门。
10. 完成 BYKC/CGYY/LibBook 查询和写操作确认门。
11. 跑局部测试、live L1、全量离线测试。

## 每阶段测试策略

- 修改 parser：跑对应 parser/service 单元测试。
- 修改 CLI 参数或输出：跑对应 CLI integration 测试。
- 修改 Auth/session/cookie：跑 Auth、Session、Cookie、SecureStore 相关测试。
- 修改真实协议 helper：先加 mock 响应测试，再运行 live L1。
- 修改写操作：必须有 dry-run/确认门测试，真实提交只手动执行。

## 风险与回滚

- 真实系统响应变化：parser 测试保留最小真实形状样例，字段缺失走稳定错误或空值策略。
- Session 污染：每次 live smoke 使用独立 `UBAANEXT_APP_DATA_DIR`。
- 凭据泄露：任何日志、JSON、失败消息新增字段前都要过脱敏测试。
- 非幂等重复提交：写请求不自动 retry，失败后由用户决定是否重试。
- WebVPN/Direct 差异：每个业务域在能力矩阵标明支持模式，不做隐式 fallback。
