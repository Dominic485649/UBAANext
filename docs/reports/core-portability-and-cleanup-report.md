# Core 跨平台性与保守清理报告

> 当前仓库版本阶段为 `v0.3.0`。本报告包含 v0.4+/v0.5 真实协议与平台适配方向的历史审查结论；当前稳定基线仍以 mock/offline parser、service、cache 和基础 CLI 验收为准。

## 结论

`UBAANextCore` 在构建依赖和模块边界上基本跨平台：核心 target 只直接依赖 `nlohmann-json`，真实网络、加密、安全存储、Cookie 持久化和系统路径能力由 `platform/common/*` 与 `platform/<os>/*` 适配层承担。当前没有证据表明应删除 `curl`、`OpenSSL`、`Catch2` 或 `nlohmann-json`；这些依赖分别支撑真实协议、加密、测试和 JSON 协议解析。

本轮清理应保持保守：保留真实认证/协议路线、测试依赖和业务 service，优先修正文档口径、依赖说明、构建 preset 表述，以及 core 内可避免的平台泄漏。

## Core 是否跨平台

### 已满足的边界

- `core/CMakeLists.txt` 将 `UBAANextCore` 构建为独立静态库，直接公开链接 `nlohmann_json::nlohmann_json`。
- `platform/common/curl` 将 libcurl 封装为网络栈和 Cookie store。
- `platform/common/openssl` 将 OpenSSL 封装为 crypto provider。
- `platform/windows`、`platform/linux`、`platform/harmony` 分别承载平台能力声明、安全存储和 app-data 路径等适配。
- `apps/cli/src/PlatformContextFactory.cpp` 负责把 CLI 与当前平台适配层组合起来，避免 core 直接知道 OS SDK。

### 本轮已消除的可避免泄漏

- core service/parser 中散落的 `_WIN32`、`localtime_s`、`gmtime_s`、`timegm`、`_mkgmtime` 分支已集中到 `UBAANext::local_time`、`UBAANext::utc_time`、`UBAANext::utc_time_t`。
- core service 中 Windows 浏览器风格 `User-Agent` 已替换为产品中性的 `UBAANext/0.4` 常量。

### 合理的平台 only 情况

这些代码属于无法完全跨平台或必须由平台实现的边界，不应删除：

| 位置 | 原因 |
| --- | --- |
| `platform/windows/src/DpapiSecureStore.cpp` | Windows DPAPI 是 Windows 凭据保护能力。 |
| `platform/linux/src/SecretServiceSecureStore.cpp` | Linux Secret Service 是 Linux 桌面凭据能力。 |
| `platform/harmony/src/*` | HarmonyOS app-data/capability 需要平台适配。 |
| `platform/common/curl/*` | 网络能力通过 libcurl 统一适配，避免 core 直接依赖平台网络 API。 |
| `platform/common/openssl/*` | 加密能力通过 provider 适配，避免 core 直接调用 OpenSSL。 |

## 代码质量评估

### 优点

- Core/platform shell 边界已经建立，平台能力通过接口组合进入 CLI。
- 真实认证、BYXT、app.buaa、SPOC、Judge、签到、阳光打卡、场馆、图书馆等业务域有清晰 service 层入口。
- Mock、unit tests、integration tests、live smoke gates 分层存在，有利于离线回归和真实协议验证隔离。
- 写操作命令普遍要求 `--confirm`/`--yes`，方向上符合高风险操作的安全策略。

### 问题

- `apps/cli/src/main.cpp` 仍集中了承载参数解析、help、JSON 描述、dispatch 和大量 command handler 的职责，后续应按命令域拆分。
- `core/src/Service/FeatureService.cpp` 同时承担 mock data、真实协议占位、feature routing 和 NotImplemented fallback，后续应按业务域或 read/write 路径拆分。
- 部分文档曾停留在 v0.1/无真实 API 口径，与当前真实认证和多业务 service 不一致。
- 构建 preset 仍需要谨慎表达工具链要求，尤其是 Visual Studio 生成器版本和 OpenHarmony 本地 SDK 环境。

## 与原 UBAA 后端功能差距（不含 UI）

仓库没有完整的原 UBAA 功能对照表，因此这里按当前 UBAANext 后端功能面与原项目语义做保守评估。

| 业务域 | 当前状态 | 主要差距 |
| --- | --- | --- |
| 认证 / session | 已有真实 SSO/UC 登录、session 恢复和分系统激活。 | 仍需持续验证各下游系统 session 过期与恢复语义。 |
| 课表 / 考试 / 空教室 / 学期 / 周次 / 成绩 | 已有 service 与 CLI 入口。 | 真实字段漂移、错误语义和 live smoke 覆盖需继续补齐。 |
| SPOC / Judge | 已有 service、parser、CLI 入口。 | 真实协议稳定性、详情页解析和批量路径仍需真实样本回归。 |
| Todo / app.buaa 聚合 | 已有聚合入口和 app.buaa session 支撑。 | 子系统覆盖和协议稳定度需要逐项验收。 |
| 签到 / 阳光打卡 | 已有只读与写操作入口。 | 写操作必须继续保持显式确认和 live-only 验证，不能默认自动执行。 |
| 博雅课程 / 场馆预约 / 图书馆座位 / 评教 | 已有 service 和高风险写操作建模。 | 与原 UBAA 的差距主要是线上稳定性、异常语义、幂等/重复提交防护和真实回归覆盖。 |
| 上传/附件类能力 | 已有 byte payload 抽象。 | 尚不能证明所有原 UBAA 上传类业务已经完整迁移。 |

总体差距不是“业务域名称缺失”，而是：真实协议成熟度、session 分域、parser 对字段漂移的容错、live smoke 覆盖、写操作安全门和正式功能对照文档。

## 外部依赖评估

| 依赖 | 是否保留 | 原因 |
| --- | --- | --- |
| `nlohmann-json` | 保留 | core parser、service、CLI JSON 输出大量依赖。 |
| `Catch2` | 保留 | unit/integration 测试依赖，删除会削弱回归能力。 |
| `curl` | 保留 | 真实网络、Cookie、WebVPN/direct 请求路径依赖。 |
| `OpenSSL` | 保留 | AES/MD5/RSA 等真实协议加密能力依赖。 |

只有在明确决定退回 mock-only、放弃真实网络和认证路线时，才应重新讨论删除 `curl`/`OpenSSL`。

## 后续建议

1. 继续保持 core 不直接链接平台 SDK、curl 或 OpenSSL。
2. 对新增 core 代码增加静态检查：不得引入 `_WIN32`、`Windows NT`、`windows.h`、`localtime_s`、`gmtime_s` 等平台痕迹。
3. 将 `apps/cli/src/main.cpp` 拆为 parser/help/dispatch/command domains。
4. 将 `FeatureService` 的 mock、真实占位和业务路由拆开。
5. 持续维护正式原 UBAA 后端差异报告，使用 `Aligned`、`ReadOnlyCandidate`、`PartiallyMigrated`、`MockOnly`、`Placeholder`、`NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified` 标注真实状态。当前入口见 `docs/reports/original-ubaa-backend-feature-matrix.md`。
