# TD CLI 使用指南

TD CLI 是 UBAANext 对 `reference/AutoTD` 核心能力的 C++ 重写入口，只覆盖命令行与底层服务能力，不迁移 AutoTD 的 Web UI、本地 Web 管理端、静态页面、浏览器打开逻辑、Cloudflare Pages 管理面板或维护者 telemetry 后台。

## 安全边界

- 默认自动化测试使用 mock client / mock transport，不访问真实校园网或真实 TD 服务器。
- `td count --refresh`、`td run --once`、`td scheduler once`、`td scheduler watch` 都可能触发真实 TD 请求，必须逐次确认：`--confirm`、`--yes`、`-y` 或交互输入 `y`。
- `td init`、`td image add`、`td image delete`、`td user add`、`td user delete`、`td scheduler clear-errors` 会写入本地状态，也必须确认。
- `--json` 模式不会交互询问；缺少确认参数时会 fail-closed 返回 `InvalidArgument`。
- `.env` 只能用于本地受控 live smoke，不得提交，不得在日志、错误、测试输出或 GitHub 中出现明文账号密码。
- 真实打卡类操作不要放入默认 CI、批处理循环或无人值守 scheduler；需要真实执行时必须手动确认具体命令和副作用范围。

## 数据目录

TD 数据保存在 UBAANext 应用数据目录的 `td/` 子目录中。可通过 `UBAANEXT_APP_DATA_DIR` 指向临时目录进行隔离验证。

典型结构：

```text
<td-root>/
  config.json
  users.json
  settings.json
  state.json
  images/
  logs/
```

其中 `images/` 保存本地图片副本，`logs/<yyyy-MM-dd>.log` 记录 scheduler tick 和状态推进摘要。

## 初始化

```powershell
ubaa td init --confirm
ubaa td init --confirm --json
```

初始化会创建默认配置、用户文件、设置文件、状态文件、图片目录和日志目录。已有文件不会被无提示覆盖。

## 图片管理

添加图片：

```powershell
ubaa td image add .\in.jpg --name in.jpg --confirm
ubaa td image add .\out.jpg --name out.jpg --confirm
```

覆盖已有图片：

```powershell
ubaa td image add .\in-new.jpg --name in.jpg --overwrite --confirm
```

列出图片：

```powershell
ubaa td image list
ubaa td image list --json
```

删除图片：

```powershell
ubaa td image delete in.jpg --confirm
ubaa td image delete in.jpg --force --confirm
```

图片名称不能包含目录，源路径必须存在并且是普通文件。删除前会校验安全文件名；如果图片仍被任一 TD 用户的入口或出口图片引用，默认拒绝删除，只有显式传 `--force` 才允许删除。CLI 不解析或预览图片内容。

## 用户管理

快速添加用户：

```powershell
ubaa td user add 2023123456 --quick 沙河 --confirm
ubaa td user add 2023123456 --quick 学院路 --rounds 2 --wait-min 5 --wait-max 10 --confirm
```

`--quick` 支持 `沙河`、`shahe`、`sh`、`学院路`、`本部`、`xueyuanlu`、`xyl`，会从默认机器列表和当前图片列表中选择入口/出口机器与图片。

显式添加用户：

```powershell
ubaa td user add 2023123456 `
  --card-id 78E0ABCD `
  --entrance 8 --exit 11 `
  --entrance-image in.jpg --exit-image out.jpg `
  --rounds 3 --wait-min 180 --wait-max 240 `
  --confirm
```

如果 `--card-id` 为空，模型层会按规则将学号十进制数字转换为十六进制大写作为 `card_id`。

查看用户：

```powershell
ubaa td user list
ubaa td user show 2023123456
ubaa td user list --json
```

删除用户：

```powershell
ubaa td user delete 2023123456 --confirm
```

## 次数查询

读取本地缓存：

```powershell
ubaa td count
ubaa td count 2023123456
```

刷新服务器次数：

```powershell
ubaa td count --refresh --confirm
ubaa td count 2023123456 --refresh --confirm
```

注意：`--refresh` 复用 TD check 协议，语义上按写性质处理，因此必须逐次确认。若本地或服务器返回次数达到 `32`，状态会标记为 `completed` 并跳过后续打卡。

## 一次性运行

```powershell
ubaa td run --once --confirm
ubaa td run once --confirm
```

`run --once` 会遍历所有 TD 用户，执行入口 check、入口图片上传、出口 check、出口图片上传，并写入状态缓存。单个用户失败会记录错误，但不应无故中断其他用户。真实执行可能产生打卡副作用，默认测试必须使用 `--mock`。

## 调度器

单次 tick：

```powershell
ubaa td scheduler once --confirm
```

清理今日错误状态：

```powershell
ubaa td scheduler clear-errors --confirm
ubaa td scheduler clear-errors --date 2026-06-03 --confirm
```

前台持续轮询：

```powershell
ubaa td scheduler watch --poll-seconds 60 --confirm
```

`scheduler watch` 当前是前台命令，不支持 `--json`；按 `Ctrl-C` 停止。每次 tick 会根据时间窗口、用户状态和 `next_run_at` 推进状态。真实环境中不要在无人值守情况下长时间运行。

## 状态查看

```powershell
ubaa td status
ubaa td status --json
```

人类可读输出使用共享 `OutputFormatter`：列表表格显示 `Index / Title / Status / Id`，随后输出 `Details` 分组显示 `Last Message`、`Next Action`、`Next Run At`、`Term Count` 等字段。JSON 输出保持稳定 envelope：

```json
{
  "ok": true,
  "data": {
    "tdStates": []
  },
  "error": null
}
```

## Mock 与真实 smoke test

无副作用验证示例：

```powershell
$env:UBAANEXT_APP_DATA_DIR = "$env:TEMP\ubaanext-td-mock"
ubaa td init --mock --confirm --json
ubaa td image add .\in.jpg --mock --confirm --json
ubaa td user add 2023123456 --mock --quick 沙河 --confirm --json
ubaa td scheduler once --mock --confirm --json
ubaa td status --mock
```

真实 `.env` smoke test 只允许在本地受控执行。建议先做真实登录、`whoami` 和只读服务验证；TD 写性质命令必须单独确认，不进入默认 runner。
