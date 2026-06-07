# 测试策略

> 当前仓库版本阶段为 `v0.4.0`。默认测试策略以 parser、service、cache、platform boundary、mock/offline CLI 和 CLI golden 集成测试为稳定基线；真实协议、live smoke 和写操作验证属于后续阶段或显式 opt-in 专项。

## 测试层级

| 层级 | 框架/入口 | 范围 |
| --- | --- | --- |
| 单元测试 | Catch2 | Result、parser、service、session、平台边界、真实只读契约 |
| CLI 集成测试 | Catch2 + `ubaa` 命令层 | JSON envelope、exit code、capability、参数校验、写操作确认门 |
| C ABI smoke（可选） | Catch2 + `UBAANextBindingsC` | version、capability、context、connection mode、mock/offline JSON envelope |
| Live smoke（默认关闭） | `tests/integration/LiveSmokeTests.cpp` / `tools/live-smoke.ps1` | 显式启用后的真实服务 smoke；L1 只读优先，写操作不自动执行 |

## 当前重点

- 默认回归不需要真实账号、密码或校园网络。
- Capability 查询必须可 mock/offline 执行；测试只断言字段存在、类型和 ABI reserved 字节，不把具体平台值解释为真实能力完成。
- 真实协议相关测试优先覆盖只读路径：登录/session、term、week、course、exam、classroom、grade、app.buaa、todo、SPOC、Judge、签到状态、阳光打卡概览、BYKC/CGYY/LibBook 查询。
- Todo 等聚合类只读接口必须保留 source-level error / partial failure 记录，不得把下游失败吞成空列表或成功结果；来源错误消息必须使用 core 共享脱敏工具。
- Judge 批量详情必须保留成功详情，并用 `status=error` 单项记录表达失败；SPOC 当前没有批量详情 API，保持 `Unverified`，不得硬造假批量协议。
- CLI 与 C ABI serializer 必须原样保留 `FeatureRecord.status` 与 `FeatureRecord.fields`；`status="error"` 项是有效数据，不等价于整体 envelope 失败。
- 写操作（签到、评教提交、选课、预约、取消、打卡提交等）必须继续要求 `--confirm` / `--yes` / `-y` 或交互输入 `y` 确认，并同时要求平台 `write_operations` capability，不进入默认 live smoke。
- typed 上传写操作（例如 `ygdk submit --photo`）需要覆盖本地文件读取失败、文件名/内容不泄露、缺少确认时 JSON 模式 fail-closed；`file upload` 占位接口必须证明不会读取文件或触发远端请求。
- `FeatureService` 真实模式不得作为字符串路由写入口；真实写 contract 通过 typed service 和 `WriteOperationGate` 覆盖。
- 测试输出和错误信息不得泄露 username、password、cookie、token、ticket、session、captcha、authorization、上传文件名、敏感 URL query、raw HTML、本地路径、锁码、预约、打卡或座位敏感原文。

## 关键测试文件

| 文件 | 重点 |
| --- | --- |
| `tests/unit/P1ReadonlyContractTests.cpp` | 真实只读契约、显式参数要求、session expired、redaction、redirect 行为 |
| `tests/unit/TodoServiceTests.cpp` | Todo 聚合字段、pending 过滤、source-level error / partial failure 语义和来源错误脱敏 |
| `tests/unit/JudgeServiceTests.cpp` | Judge 列表过滤、redirect、批量详情 partial failure 与错误脱敏 |
| `tests/unit/WriteOperationGateTests.cpp` | CLI/API 确认与平台 `write_operations` 双重写门控 |
| `tests/unit/PlatformBoundaryTests.cpp` | Core/platform 分层、C ABI status、capability reserved、partial failure serializer 边界 |
| `tests/integration/CliIntegrationTests.cpp` | CLI JSON envelope、help/golden、capability、参数校验、写操作确认门 |
| `tests/integration/CAbiSmokeTests.cpp` | 启用 bindings 时的 C ABI version/capability/context/mock-offline smoke |
| `tests/integration/LiveSmokeTests.cpp` | 显式启用的 live smoke gate |
| `tests/unit/SessionGuardsTests.cpp` | 登录失效和 SSO 响应识别 |
| `tests/unit/SecurityRedactionTests.cpp` | CLI/core 共享敏感信息脱敏，覆盖 URL query、headers、本地路径、移动端路径、raw HTML 和业务敏感字段 |
| `tests/unit/*ServiceTests.cpp` / `tests/unit/*ParserTests.cpp` | 各业务域 service/parser 边界 |

## 运行默认离线测试

```powershell
cmake --preset windows-ninja-msvc-debug
cmake --build --preset windows-ninja-msvc-debug
ctest --preset windows-ninja-msvc-debug --output-on-failure
```

默认测试不启用 `UBAANEXT_BUILD_LIVE_SMOKE`，不需要真实账号、真实密码或校园网。

## C ABI / NAPI boundary smoke

C ABI smoke 是 NAPI/ArkTS wrapper 前置边界验证。启用 bindings 后运行：

```powershell
cmake --preset windows-ninja-msvc-debug -DUBAANEXT_BUILD_BINDINGS=ON
cmake --build --preset windows-ninja-msvc-debug --target UBAANextCAbiSmokeTests
ctest --preset windows-ninja-msvc-debug --output-on-failure -R "C ABI"
```

该 smoke 覆盖：

- `ubaanext_version()` 非空。
- `ubaanext_get_capabilities(nullptr)` 的具名 status。
- `UbaaNextCapabilities.reserved` 全部置零。
- context create/release 和非法 mode。
- mock 构建下 `mock` mode 的只读 JSON envelope，例如 `ubaanext_terms(...)`。

NAPI smoke 应建立在同一合同上：version、capability object、mock/offline readonly API、`UbaaError` 映射、partial failure item 不被过滤。不得运行真实登录、真实网络或真实写。

## Live smoke 策略

Live smoke 仅在显式设置环境变量时运行，例如：

```powershell
$env:UBAANEXT_LIVE = '1'
$env:UBAANEXT_USERNAME = '<username>'
$env:UBAANEXT_PASSWORD = '<password>'
.\tools\live-smoke.ps1 -Level L1
```

L1 只运行读取类命令；任何未标记为跳过的读取命令失败都必须使 runner 失败，不能作为“已知失败”静默通过。当前 runner 覆盖 term/week/course/exam/grade/app/todo/SPOC/Judge/signin/YGDK、BYKC `profile/courses/chosen/stats`、direct-only CGYY `sites/orders/order lock-code`、LibrarySeat `libraries/reservations`，以及课堂资源 `live week/resources/detail`。课堂资源真实下载默认跳过；需设置 `UBAANEXT_LIVE_DOWNLOAD=1`，并用 `UBAANEXT_LIVE_DOWNLOAD_INCLUDE=ppt` 或 `ppt,video` 控制范围。L2/L3 写操作需要额外环境变量和用户手动指定具体命令；runner 不自动执行真实写操作。

课堂资源 smoke 报告必须区分：缺少凭据、校园网/后端不可达、指定日期资源为空、资源缺少 PPT GUID、视频是 HLS 但本机没有 `ffmpeg`、下载后只生成 `.m3u8.url` sidecar、以及本地临时目录清理失败。默认 `ctest` 不访问真实课堂资源后端。

## Mock 策略

- MockHttpClient / MockCacheStore / fixture 数据用于默认离线回归。
- Mock 响应应覆盖空数据、字段缺失、类型漂移、业务错误 body、登录失效页、cookie/session 持久化失败、上传文件读取失败、聚合来源级错误脱敏、批量详情单项失败、SPOC 单详情提交信息失败降级、diagnostics 高敏感脱敏等真实协议风险。
- 新增真实协议修复时，优先补离线 mock/fixture 测试，再考虑显式 live smoke。
