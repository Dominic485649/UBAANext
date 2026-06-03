# AutoTD 核心迁移第一阶段边界

## 背景

`reference/AutoTD` 保存了 `autotd-buaa` 的 Python 源码，作为 BUAA TD 场景的协议和业务规则参考。迁移目标不是逐行翻译 Python，也不是复刻 Web UI，而是在 `UBAANext` 现有 C++ core/platform/CLI 架构内重写可复用、可测试、跨平台的 TD 核心能力。

## 不迁移范围

- `autotd ui` 本地 Web 管理端。
- `ui_static/` 前端资源。
- Cloudflare Pages 管理面板。
- 维护者侧 telemetry dashboard。
- 任何必须依赖浏览器交互的功能。

telemetry 只作为参考能力保留，不作为第一阶段默认能力。若后续实现，必须默认关闭或显式启用，并遵守项目隐私和安全约束。

## 第一阶段迁移范围

第一阶段只实现可离线验证的核心模型、存储和 CLI 骨架：

1. TD 配置模型
   - 服务器地址、端口、超时。
   - `type`、`schoolno`、`eventno`。
   - 机器列表：`id`、`machinesn`、`location`、`doortype`。
   - 默认时间窗口：`07:30-10:00`、`11:30-14:00`、`15:30-20:00`。

2. TD 用户模型
   - `student_id`、`card_id`、入口/出口机器、入口/出口图片。
   - `rounds`、`wait_time_min`、`wait_time_max`。
   - `card_id` 为空时由学号转十六进制大写。
   - 校验轮次数、等待时间、图片名、机器编号。

3. TD 本地存储
   - 使用项目现有应用数据路径，不直接固定为 `~/.autoTD`。
   - 为 TD 模块建立独立命名空间。
   - 支持初始化默认配置、用户配置、设置、状态和图片目录。
   - 不无提示覆盖已有用户数据。

4. 图片管理
   - 支持添加和列出图片。
   - 添加时复制到 TD 图片目录。
   - 支持指定文件名和覆盖开关。
   - 使用 `std::filesystem` 处理跨平台路径。

5. quick 用户生成
   - 支持 `沙河`、`shahe`、`sh`。
   - 支持 `学院路`、`本部`、`xueyuanlu`、`xyl`。
   - 按机器 `location` 和 `doortype` 选择入口/出口池。
   - 随机选择逻辑需要可注入，便于测试。

6. 状态和限制
   - 缓存每个用户最近一次 TD 次数。
   - `TD_COMPLETION_LIMIT = 32`。
   - 到达 32 次时跳过后续写操作。

## 后续阶段迁移范围

第二阶段实现 TD socket 协议客户端：

- 请求头为 big-endian `int32 length` + `uint8 request_type`。
- check 请求类型为 `80`，payload 为 JSON。
- photo 上传请求类型为 `100`，payload 为 `machinesn_timestamp_ms` 字节串后接图片 bytes。
- 响应头同样为 `length + code`，code 必须匹配请求类型。
- 解析 `srvresp` 中的 `本学期锻炼次数[:：] <n>`。

第三阶段实现 `run --once`：

- 遍历用户。
- 入口 check、入口图片上传、等待、出口 check、出口图片上传。
- 单用户失败不中断后续用户。
- 写入状态缓存和运行结果。
- 写操作必须遵守 CLI 确认策略。

第四阶段实现平台无关调度核心：

- 根据北京时间和窗口判断是否可执行。
- 每个用户独立记录 `pending`、`waiting`、`completed`、`error`。
- 计算 `next_action`、`completed_rounds`、`due_at`、`last_message`、`last_error`。
- 先实现可测试的调度状态机，再考虑后台进程驻留。

## 建议 C++ 模块落点

- `core/include/UBAANext/Model/Td.hpp`
- `core/include/UBAANext/Service/TdService.hpp`
- `core/include/UBAANext/Storage/TdStore.hpp`
- `core/include/UBAANext/Protocol/TdClient.hpp`
- `core/src/Service/TdService.cpp`
- `core/src/Storage/TdStore.cpp`
- `core/src/Protocol/TdClient.cpp`

如果现有命名习惯更适合按领域拆分，也可以使用 `UBAANext/TD/`，但应避免将业务逻辑放在 `apps/cli/src/CommandHandlers.cpp` 中。

## CLI 美化落点

CLI 输出美化应先扩展共享 `OutputFormatter`，再复用到现有命令和未来 TD 命令：

- JSON 输出保持无颜色、结构稳定。
- 人类可读输出支持 ANSI 彩色。
- 标题、表头、成功、警告、错误、次要信息使用统一样式。
- 表格仍使用现有宽度计算和对齐逻辑。
- 后续补充 `--no-color` 或环境变量禁用颜色。
- 彩色能力必须在 Windows 和 Linux CLI 上保持一致。

TD 命令输出建议采用阶段化格式：读取配置、检查用户、入口打卡、等待或跳过、出口打卡、写入状态、汇总结果。

## 测试要求

第一阶段测试必须覆盖：

- TD 用户 JSON 往返。
- `card_id` 自动推导。
- 用户字段校验。
- 默认配置机器列表。
- quick 校区别名和入口/出口池选择。
- 图片名路径安全校验。
- 状态中 TD 次数达到 32 时跳过。
- 输出格式化在 JSON 模式下不包含 ANSI 控制码。

真实 TD 服务器请求只允许进入受控 live smoke，不进入默认测试。
