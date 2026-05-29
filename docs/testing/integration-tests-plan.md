# 集成测试计划

> 当前仓库版本阶段为 `v0.4.0`。本计划记录 v0.4 稳定集成基线和 v0.5+ 后续真实测试方向；默认集成测试以 mock/offline CLI、parser/service/cache、命令树、JSON envelope、exit code 和写操作确认门契约为准，live smoke 与 OpenHarmony/binding 验证属于显式 opt-in 或后续阶段。

## 目标

集成测试用于验证 CLI 层到 core service 的端到端契约，包括参数校验、JSON envelope、exit code、mock/offline 行为、写操作确认门和 live smoke gate。默认集成测试不得依赖真实账号、密码、校园网络或在线写操作。

当前项目还承担 DevEco/OpenHarmony native SDK 输出验证：Windows/host 继续验证 CLI 和 CTest，OpenHarmony preset 验证 core/platform/binding 可构建且不强制构建 CLI。

## 当前集成测试入口

| 文件 / 工具 | 范围 |
| --- | --- |
| `tests/integration/CliIntegrationTests.cpp` | 离线 CLI 集成测试，覆盖命令分发、JSON 输出、参数错误、固定 exit code、help golden 合同、config/cache 确认门、写操作未确认时拒绝执行等。 |
| `tests/integration/LiveSmokeTests.cpp` | 显式 opt-in 的 live smoke gate，默认跳过。 |
| `tools/live-smoke.ps1` | 手动 live smoke runner；L1 为只读，L2/L3 不自动执行写操作。 |
| `openharmony-clang-*` CMake presets | OpenHarmony native 构建入口；关闭 CLI/tests，启用 binding，用于验证 DevEco 可复用的 native target。 |
| `UBAANextBindingsC` | 最小 C ABI smoke target；当前只验证 version/capability，不代表真实登录或真实写完成。 |
| package consumer smoke | 外部 CMake 消费验证；通过 `find_package(UBAANext CONFIG REQUIRED)` 链接 `UBAANext::UBAANextBindingsC`，并显式提供 SDK 同源依赖前缀。 |

## 只读优先策略

集成测试和 live smoke 的推进顺序应与 `docs/reports/original-ubaa-backend-feature-matrix.md` 保持一致：

1. 登录/session 基础：redirect、session expired、redaction、错误分类。
2. 教务/成绩只读：term、week、course、exam、classroom、grade。
3. app.buaa 生态只读：app version、announcement、todo、SPOC、Judge、signin today、YGDK overview/records。
4. 预约生态只读：BYKC `profile/courses/chosen/stats`、direct-only CGYY `sites/orders/order lock-code`、LibBook `libraries/reservations`。
5. 写操作仅验证“未确认时拒绝执行”“平台未声明 `write_operations` 时拒绝执行”和“需要显式确认”的本地契约；不默认真实执行。

## Live smoke 规则

- 默认跳过：必须设置 `UBAANEXT_LIVE=1`。
- 需要真实凭据时，必须通过环境变量传入，且输出要经过脱敏。
- L1 只允许读取类命令；除显式 skip 的命令外，读取失败必须让 runner 返回失败退出码。
- L2/L3 即使启用，也不由 runner 自动执行真实写操作；用户必须手动指定具体命令并确认风险。
- CGYY 当前保持 direct-only 约束；非 direct 模式下应显式跳过 `sites/orders/order lock-code`，而不是隐式改路由。

## 验收标准

- CLI JSON 输出保持 `ok/data/error` envelope，`help --json` 的命令目录不得出现重复命令名。
- 参数错误、认证失效、网络错误、解析错误和业务错误不互相混淆。
- 写操作缺少 `--confirm` / `--yes` / `-y` 且无法交互确认时必须失败且不触发远端请求；未知或未验证平台缺少 `write_operations` capability 时也必须失败。
- 聚合类只读命令遇到单个来源失败时，应保留成功来源并输出 source-level error，且错误消息必须脱敏，而不是把失败吞成空列表或整体成功。
- Judge 批量详情遇到单个详情失败时，应保留成功详情并输出 `status=error` 单项记录；SPOC 当前没有批量详情 API，集成测试不得假设其存在。
- 测试输出不得泄露 username、password、cookie、token、ticket、session、captcha、authorization、上传文件名、敏感 URL query、raw HTML、本地路径、锁码、预约、打卡或座位敏感原文。
- 新增 live 覆盖必须先有离线 contract 或 parser/service 测试保护；若只存在单详情 API，不得用 CLI smoke 伪造批量语义。
- `file upload` 作为占位接口只验证稳定 `NotImplemented` 合同，不得读取 `--path` 指向的本地文件。
- `ygdk submit --photo` 等 typed 上传写操作必须单独验证本地文件读取失败、缺少确认时 fail-closed，且不进入默认 live smoke。
- OpenHarmony native 构建通过只说明 core/platform/binding 可被 DevEco 复用，不说明 HAP、NAPI、真实登录或真实业务 live 已完成。
- `UBAANextBindingsC` 当前只允许 version/capability smoke；后续新增只读 C ABI/NAPI API 前必须先补错误码、redaction 和 partial failure 回归。
- package consumer smoke 必须验证安装后的 headers、targets、DLL/import library 和依赖 target 同时可解析；CURL/OpenSSL/nlohmann-json 的消费侧来源必须与 SDK ABI/triplet 匹配，否则应报告为依赖环境问题，不把 package 本身标记为失败。
