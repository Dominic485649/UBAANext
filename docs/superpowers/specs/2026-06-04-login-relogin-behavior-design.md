# CLI login / relogin 行为调整设计

日期：2026-06-04

## 背景

当前 CLI 登录相关命令同时存在多套写法：

- `login <账号> <密码> --save-password`
- `login --relogin <账号> <密码>`
- `relogin <账号> <密码>`
- `relogin --saved`

这增加了用户理解成本。用户期望改为：`login` 默认保存账号密码，`relogin` 直接复用 `login` 保存的内容，不再需要 `--saved` 或再次传账号密码。

## 目标

1. `login <账号> <密码>` 登录成功后默认保存账号密码。
2. `relogin [-y|--confirm|--yes]` 复用已保存账号密码重新登录。
3. 取消推荐并拒绝旧写法：
   - `login --relogin <账号> <密码>`
   - `login <账号> <密码> --save-password`
   - `relogin <账号> <密码>`
   - `relogin --saved`
4. Linux 端自行实现安全存储；不可用时允许明文 fallback。
5. HarmonyOS 端本轮不实现安全存储。
6. human-readable CLI 输出按业务重要性增加彩色强调；JSON 输出保持纯数据。

## 非目标

- 不实现 HarmonyOS 安全存储。
- 不新增真实写 UI。
- 不改变 `--json` 输出字段结构。
- 不把明文 fallback 解释为安全存储能力完成。

## 命令行为

### login

推荐命令：

```powershell
ubaa login <账号> <密码>
```

行为：

1. 若已有本地登录会话，仍拒绝直接覆盖，并提示使用 `relogin --confirm`。
2. 登录成功后自动保存：
   - `login.username`
   - `login.password`
   - `login.connection_mode`
3. 保存优先走平台安全存储。
4. 若安全存储不可用，允许 fallback 到明文/弱存储，并在 human 输出或文档中提示风险。
5. 不再接受 `--save-password`。
6. 不再接受 `--relogin`。

### relogin

推荐命令：

```powershell
ubaa relogin --confirm
```

行为：

1. 不接收账号和密码位置参数。
2. 不接收 `--saved`。
3. 默认读取 `login` 保存的账号密码。
4. 因为会清理旧 session/cookie/cache 并替换本地会话，仍要求 `-y` / `--confirm` / `--yes`。
5. 若没有保存凭据，返回 `InvalidArgument`，提示先执行 `login <账号> <密码>`。
6. 成功后继续保存当前凭据和 connection mode，保证下一次 `relogin` 可复用。

## 存储设计

### Windows

继续使用现有 DPAPI `DpapiSecureStore`。这是系统级加密存储，可用于保存账号密码。

### Linux

新增 CLI 侧 Linux 安全存储实现，优先级如下：

1. 优先使用自实现安全存储。
2. 若安全存储不可用或初始化失败，fallback 到 `PlainFileStore`。
3. 明文 fallback 允许登录继续成功，但必须作为风险状态记录在文档和 human-readable 输出中。

实现应保持 `ISecureStore` 接口，不把存储细节泄露给 AuthService。

### HarmonyOS

本轮不实现安全存储。HarmonyOS 端能力仍应保持 fail-closed / volatile，不把它解释为真实登录 UI 完成。

## 彩色输出设计

彩色标注不限于 ID，而是按人眼扫读时最重要的信息判断。

规则：

- 只影响 human-readable CLI 输出。
- `--json` 永远不包含 ANSI 颜色。
- 敏感内容先脱敏，再决定是否上色。
- 不把整张表染花；优先强调每行关键字段。

颜色建议：

- 红色 / 高亮红：错误、失败、过期、危险动作、缺失确认、不可用能力。
- 黄色 / 橙色：待处理、需要确认、fallback 到明文存储、风险提示。
- 绿色：成功、已登录、已保存、可用能力、通过状态。
- 青色 / 蓝色：定位类字段，如 `StudentId`、`Id`、`Code`、`CourseCode`、`termCode`、`bookingId`。
- 紫色 / 品红：名称类重点，如课程名、用户显示名、服务名称、能力组标题。
- 加粗 / 高亮：核心结果字段，如登录成功消息、课程名、当前周次、考试时间、成绩结果、状态摘要。

示例：

- 登录结果：`Message=登录成功。` 绿色/加粗；`DisplayName` 品红；`StudentId` 青色。
- 课程表：`Name` 高亮；`Time` / `Sections` 醒目；`Classroom` 青色；`Code` / `Id` 蓝色。
- Capability：`true` 绿色；关键 `false` 红色或黄色；`liveLogin=false`、`secureStore=false`、`writeOperations=true` 需要醒目。
- Partial failure：`status=error` 红色；`source-error`、`errorCode`、`errorMessage` 红/黄组合。

## 错误处理

- 旧写法返回 `InvalidArgument`，提示新写法。
- 没有保存凭据时，`relogin` 返回 `InvalidArgument`。
- 安全存储失败但允许明文 fallback 时，不应导致登录失败；应记录风险提示。
- 真实登录失败时，不保存新密码，避免保存错误凭据。
- 所有错误消息继续脱敏。

## 测试计划

更新或新增 CLI integration 测试：

1. `login <账号> <密码>` 成功后默认保存凭据。
2. `relogin --confirm` 不带 `--saved` 也能复用保存凭据。
3. `relogin --saved` 返回 `InvalidArgument`。
4. `relogin <账号> <密码>` 返回 `InvalidArgument`。
5. `login --relogin ...` 返回 `InvalidArgument`。
6. `login ... --save-password` 返回 `InvalidArgument`。
7. help / help JSON 不再展示旧写法。
8. human-readable 输出包含颜色，`--json` 不包含 ANSI 颜色。

## 文档更新

需要同步：

- CLI help 文本。
- JSON/CLI 输出文档。
- 测试策略文档。
- 如已有登录文档，更新 login/relogin 示例。

## 安全说明

- Windows DPAPI 保存是加密保存。
- Linux 优先安全存储；fallback 明文是用户批准的兼容行为，不等价于安全存储能力完成。
- HarmonyOS 本轮不实现安全存储，不进入真实登录 UI。
- 不在日志、错误、测试输出中泄露密码、cookie、token 或 session。
