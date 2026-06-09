# 真实 smoke test

本文记录登录、课堂资源、Cloud 和 TD 相关真实环境 smoke test 的安全流程。该流程只用于人工受控验证，不进入默认 CI，也不作为普通自动化测试的一部分。

## 安全原则

- `.env` 只在本地读取，不提交、不打印、不复制到日志。
- 命令输出、测试摘录和最终报告不得包含明文密码、cookie、token、session、完整敏感账号、上传文件名或本地完整路径。
- 默认自动化测试必须继续使用 mock client、mock transport 和隔离 app data，不访问真实校园网、真实云盘、真实课堂资源或真实 TD 服务器。
- 真实写类命令必须逐次确认，不能批量执行，不能在无人值守循环中运行。
- 如果网络、账号状态或服务器响应不符合预期，不得为了通过 smoke test 放宽 `--confirm`、`WriteOperationGate` 或平台 capability。

## 环境变量

项目根目录 `.env` 可提供真实 smoke test 需要的账号密码，但读取时不得回显内容。可从 `.env.example` 复制后只在本机填写。

通用变量：

- `UBAANEXT_LIVE=1`：启用课堂直播/课堂资源只读 smoke。
- `UBAANEXT_CLOUD=1`：启用北航云盘只读 smoke。
- `UBAANEXT_USERNAME`：真实登录账号。
- `UBAANEXT_PASSWORD`：真实登录密码。
- `UBAANEXT_APP_DATA_DIR`：隔离 app data 目录；为空时 runner 会创建临时目录。
- `UBAANEXT_CONNECTION_MODE=direct|vpn`：连接模式，默认 direct。
- `UBAANEXT_CLI_PATH`：可选，覆盖 CLI 路径。
- `UBAANEXT_LIVE_LEVEL=L1|L2|L3`：人工记录本次 smoke 范围，脚本参数默认读取该值。

课堂资源变量：

- `UBAANEXT_LIVE_START_DATE` / `UBAANEXT_LIVE_END_DATE`：覆盖 `live week` 周范围。
- `UBAANEXT_LIVE_RESOURCE_DATE`：覆盖 `live resources` 日期。
- `UBAANEXT_LIVE_RESOURCE_STATUS=live|playback|generating|all`：课堂资源状态过滤。
- `UBAANEXT_LIVE_DOWNLOAD=1`：显式开启真实课堂资源下载。
- `UBAANEXT_LIVE_DOWNLOAD_INCLUDE=ppt|video|ppt,video|all`：下载内容，建议首次只用 `ppt`。

Cloud 变量：

- `UBAANEXT_CLOUD_ROOT_KIND=user|shared|department|group|all`：选择 roots/root smoke 的根目录类型。
- `UBAANEXT_CLOUD_WRITE=1`：显式开启 Cloud 可逆写 smoke。
- `UBAANEXT_ALLOW_WRITE=1` 与 `UBAANEXT_CONFIRM_WRITE=1`：Cloud 可逆写 smoke 的额外安全门。
- `UBAANEXT_CLOUD_WRITE_CLEAN_RECYCLE=1`：在可逆写清理阶段尝试清空回收站中的测试目录；默认不做永久删除。

桌面诊断变量：

- `UBAANEXT_DESKTOP_PATH`：可选，手动指向 `ubaa.exe` 或 `ubaa-gui` 用于记录 `--diagnose`；当前 `tools/live-smoke.ps1` 不自动启动 GUI。

## 推荐最小验证范围

优先选择最小副作用路径：

1. 确认 `.env` 存在但不打印内容。
2. 将 `.env` 中的账号密码安全加载到当前进程环境。
3. 使用隔离的 `UBAANEXT_APP_DATA_DIR` 运行真实登录。
4. 运行 `whoami` 或等价会话恢复验证。
5. 运行只读命令，确认真实网络与会话可用。
6. 检查输出和本地日志不包含密码、cookie、token、完整账号、完整路径或上传文件名。

## 脚本入口

PowerShell runner：

```powershell
.\tools\live-smoke.ps1 -Level L1
```

默认行为：

- 未设置 `UBAANEXT_LIVE=1`、`UBAANEXT_CLOUD=1` 或 `UBAANEXT_CLOUD_WRITE=1` 时直接跳过并退出 0。
- 必须提供 `UBAANEXT_USERNAME` 和 `UBAANEXT_PASSWORD`。
- 先执行登录、`whoami`、`term list`。
- `UBAANEXT_LIVE=1` 时执行课堂直播周课表、课堂资源列表，并在找到可用资源时执行 detail/download 相关只读或受控下载路径。
- `UBAANEXT_CLOUD=1` 时执行 Cloud roots/root/list/size/recycle/shares 等只读路径。
- `UBAANEXT_CLOUD_WRITE=1` 时创建临时目录和文件、上传、列出、容量、重命名、分享创建/删除、下载 URL，然后删除测试目录；需要额外写门变量。

## Cloud mount 与桌面跳过项

当前桌面 UI 有 Cloud mount 按钮和 `--diagnose` 输出，但 Runtime 尚未注册 WinFsp、Cloud Files 或 FUSE adapter。真实 smoke 不应宣称以下能力通过：

- Windows 盘符实际挂载。
- Windows Cloud Files sync root 注册。
- Linux FUSE 实际挂载到用户目录。
- 文件管理器写回云盘。

可记录的桌面验证仅限：

- `ubaa.exe --version` / `ubaa-gui --version`。
- `ubaa.exe --diagnose` / `ubaa-gui --diagnose`。
- GUI 在 `--mock` 下能打开并显示诊断、Cloud 列表按钮和 mount 不可用状态。

未验证原因应写明：mount adapter 尚未接入 Runtime，Windows adapter 源文件缺失，Linux FUSE low-level 封装尚未注册到 `CloudMountManager`。

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
- Cloud 可逆写 smoke 是否执行；如未执行，应写明默认跳过或未显式开启写门。
- TD 远端写类命令是否执行；如未执行，应写明“因副作用风险按安全策略跳过”。
- 桌面 GUI 或 mount 是否验证；如未验证，应写明缺少构建产物、缺少图形环境、缺少 Slint 构建或 adapter 未完成。
- 若失败，区分代码问题、网络环境问题、校园网限制、账号状态问题、服务器响应变化、安全 gate 拒绝或外部依赖缺失。

不得记录：

- `.env` 原文。
- 明文账号密码。
- cookie、token、session。
- 用户图片路径或图片内容。
- 上传文件名、本地完整路径或分享 token。
- 可识别个人隐私的完整状态日志。
