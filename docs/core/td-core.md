# TD 核心模块开发说明

TD 核心模块是 AutoTD 核心能力在 UBAANext 中的 C++ 分层重写。实现目标是功能等价、可测试、跨平台、可审计，而不是逐行翻译 Python。

## 模块边界

| 层级 | 主要文件 | 职责 |
| :--- | :--- | :--- |
| 模型层 | `core/include/UBAANext/Model/Td.hpp`、`core/src/Model/Td.cpp` | TD 配置、机器、用户、状态、quick 校区选择、JSON 往返、`card_id` 推导。 |
| 存储层 | `core/include/UBAANext/Storage/TdStore.hpp`、`core/src/Storage/TdStore.cpp` | 初始化 `td/` 数据目录，读写 config/users/settings/state，复制和列出图片。 |
| 协议层 | `core/include/UBAANext/Protocol/TdClient.hpp`、`core/src/Protocol/TdClient.cpp` | 构造 TD check/photo 协议帧，解析服务器响应，抽象 `ITdTransport` / `ITdClient`。 |
| 服务层 | `core/include/UBAANext/Service/TdService.hpp`、`core/src/Service/TdService.cpp` | `run_once` 编排、次数上限跳过、状态写入、错误隔离。 |
| 调度层 | `core/include/UBAANext/Service/TdSchedulerService.hpp`、`core/src/Service/TdSchedulerService.cpp` | 时间窗口判断、状态机推进、日志追加、错误清理。 |
| 平台 TCP | `platform/common/tcp/` | 生产 TD TCP transport，处理 connect/send/recv/timeout/error 映射。 |
| CLI 壳 | `apps/cli/src/main.cpp`、`apps/cli/src/CommandHandlers.cpp` | 参数解析、安全确认、mock/real client 选择、调用 core service、格式化输出。 |

CLI 不应承载 TD 业务规则；新增规则优先放入模型、协议、服务或调度层。CLI 只负责把用户输入转换为服务参数，并通过 `OutputFormatter` 展示结果。

## 协议说明

TD socket 帧格式为：

```text
4 字节 big-endian body length + 1 字节 request type + body
```

当前请求类型：

- `80`：check 请求，body 为 JSON。
- `100`：photo 上传请求，body 为 `machinesn_timestamp_ms` 字节串后接图片 bytes。

生产 transport 返回完整响应帧，协议客户端再解码帧头、校验 body 长度、解析 JSON，并从 `srvresp` 文本中提取 `本学期锻炼次数`。

`query_count` 复用 check 协议，但 TD 服务器语义具有写性质，因此调用方必须像写操作一样经过确认 gate。

## 状态机

每个用户每日独立维护 `UserState`：

| 字段 | 含义 |
| :--- | :--- |
| `status` | `pending`、`waiting`、`completed`、`error` 等。 |
| `next_action` | 下一步动作：`entrance`、`exit`、`none`。 |
| `completed_rounds` | 今日已完成轮数。 |
| `term_count` | 最近一次本学期锻炼次数缓存。 |
| `next_run_at` | 等待出口或下一轮入口的 ISO 时间。 |
| `last_error` | 最近错误；error 状态会保持到手动清理。 |
| `last_message` | 最近成功、等待、跳过或诊断消息。 |

关键规则：

- `completion_limit` 为 `32`；本地缓存或服务器返回次数达到上限时标记 `completed` 并跳过远程请求。
- 窗口外不发送 TD 远程请求；已到期动作会作废并回到下一窗口入口。
- `waiting` 且未到期时只返回剩余秒数，不发送远程请求。
- `error` 状态不会自动重试，必须通过 `td scheduler clear-errors --confirm` 清理。
- `clear_today_errors` 也是本地写操作，core 内部同样要求 `WriteOperationGate`。

## 写操作安全门

所有会写本地状态或触发真实远端副作用的路径必须保留双层保护：

1. CLI 层调用 `confirm_sensitive_operation_or_exit`，要求 `--confirm`、`--yes`、`-y` 或交互 `y`。
2. Core 服务层调用 `require_write_operation(m_write_gate)`，校验平台 `write_operations` capability 和确认状态。

当前 WriteGated 路径包括：

- `td init`
- `td image add`
- `td user add`
- `td user delete`
- `td count --refresh`
- `td run --once`
- `td scheduler once`
- `td scheduler clear-errors`
- `td scheduler watch`

mock 模式仅用于测试和演示；mock client 不证明真实 TD 服务器可用。

## 日志与敏感信息

`scheduler` 会追加 `logs/<yyyy-MM-dd>.log`。日志只应包含用户标识、状态推进和服务器返回摘要，不得包含密码、cookie、token、`.env` 内容、图片 bytes 或原始请求帧。任何错误进入 CLI 输出前都应继续走共享脱敏链路。

TD 用户的 `student_id` 和图片文件名仍属于隐私相关信息；提交前不得提交真实用户配置、状态、日志或图片。

## 测试策略

默认测试必须离线：

- 协议测试使用 mock transport。
- 服务和调度测试使用 mock `ITdClient`。
- CLI 集成测试使用 `--mock` 和隔离 `UBAANEXT_APP_DATA_DIR`。
- 真实 TD 服务器交互只作为人工 live smoke，不进入默认 CI。

重点测试覆盖：

- 用户 JSON 往返与字段校验。
- `card_id` 自动推导。
- quick 校区别名、机器池和图片选择。
- 图片路径安全、覆盖策略、普通文件检查。
- TD 帧编码/解码、响应解析、次数提取。
- `run_once` 成功、失败、跳过和状态写入。
- scheduler 窗口判断、等待、出口、完成、错误保持、错误清理。
- CLI JSON envelope 稳定和人类可读 Details 输出。

## 真实 smoke test

真实 `.env` smoke test 只能在用户明确授权后进行。执行者可以读取 `.env` 并导出 `UBAANEXT_USERNAME`、`UBAANEXT_PASSWORD` 等环境变量，但不得打印原值。建议流程：

1. 确认 `.env` 已被 `.gitignore` 忽略。
2. 使用隔离 `UBAANEXT_APP_DATA_DIR`。
3. 先运行真实 `login`、`whoami` 和只读命令。
4. 若要执行 TD 写性质命令，必须单条命令逐次确认，不运行无人值守循环。
5. 报告时只记录命令类别、退出码和脱敏摘要，不记录账号密码。
