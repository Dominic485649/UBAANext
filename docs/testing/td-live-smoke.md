# TD 真实 smoke test

本文记录 TD/登录相关真实环境 smoke test 的安全流程。该流程只用于人工受控验证，不进入默认 CI，也不作为普通自动化测试的一部分。

## 安全原则

- `.env` 只在本地读取，不提交、不打印、不复制到日志。
- 命令输出、测试摘录和最终报告不得包含明文密码、cookie、token、session 或完整敏感账号。
- 默认自动化测试必须继续使用 mock client、mock transport 和隔离 app data，不访问真实校园网或真实 TD 服务器。
- 真实写类 TD 命令必须逐次确认，不能批量执行，不能在无人值守循环中运行。
- 如果网络、账号状态或服务器响应不符合预期，不得为了通过 smoke test 放宽 `--confirm`、`WriteOperationGate` 或平台 capability。

## 环境变量

项目根目录 `.env` 可提供真实 smoke test 需要的账号密码，但读取时不得回显内容。既有 live smoke runner 使用：

- `UBAANEXT_LIVE=1`：启用真实 smoke test。
- `UBAANEXT_USERNAME`：真实登录账号。
- `UBAANEXT_PASSWORD`：真实登录密码。
- `UBAANEXT_ALLOW_WRITE=1`：仅表示允许 runner 进入写操作阶段；TD 打卡类命令仍需单独人工确认具体命令和副作用。

## 推荐最小验证范围

优先选择最小副作用路径：

1. 确认 `.env` 存在但不打印内容。
2. 将 `.env` 中的账号密码安全加载到当前进程环境。
3. 使用隔离的 app data 目录运行真实登录。
4. 运行 `whoami` 或等价会话恢复验证。
5. 运行只读或近似只读命令，确认真实网络与会话可用。
6. 检查输出和本地日志不包含密码、cookie、token 或完整敏感凭据。

## TD 远端请求限制

以下命令可能触发真实 TD 服务器请求或真实打卡副作用，不能默认运行：

- `td count --refresh`
- `td run --once`
- `td scheduler once`
- `td scheduler watch`

如确需执行，必须在每条命令前再次说明：

- 将访问的真实服务范围。
- 是否可能新增或改变 TD 服务器记录。
- 本次只执行一次还是持续轮询。
- 需要的确认参数，例如 `--confirm`、`--yes` 或 `-y`。

`td scheduler watch` 不应作为发布前 smoke test 的默认动作；如果必须验证，只能使用短时间前台运行，并由人工随时中止。

`td image delete` 是本地图片管理命令，可在隔离 `UBAANEXT_APP_DATA_DIR` 中验证：先添加图片，再验证被用户引用时拒绝删除，删除用户后允许删除，`--force` 只用于明确接受引用失效风险的本地清理。该命令不访问真实 TD 服务器。

本项目不实现 AutoTD 的后台 daemon、自动挂后台、后台 PID 管理、自动刷新/刷取、Web 管理端或 telemetry。真实 smoke 不应尝试验证这些被排除能力。

## 结果记录

最终报告只记录：

- 执行了哪些命令类别。
- 是否使用隔离 app data。
- 登录、会话恢复和只读验证是否成功。
- TD 远端写类命令是否执行；如未执行，应写明“因副作用风险按安全策略跳过”。
- 若失败，区分代码问题、网络环境问题、校园网限制、账号状态问题、服务器响应变化或安全 gate 拒绝。

不得记录：

- `.env` 原文。
- 明文账号密码。
- cookie、token、session。
- 用户图片路径或图片内容。
- 可识别个人隐私的完整状态日志。
