# Reference 差异矩阵

> 基准更新：本轮已对主仓库、`reference/buaa-api`、`reference/UBAA`、`reference/BBUAA` 执行 `fetch` + `pull --ff-only`；均为最新。`autotd-buaa` 以本地 `0.1.15` 包源码为基线，并通过 PyPI 校验最新版本仍为 `0.1.15`。

## 范围

本矩阵记录当前 UI + CLI 完善阶段与参考项目的能力差异，重点覆盖课堂资源下载、TD 图片管理、北航云盘 CLI/Cloud VFS 与桌面 mount 前端缺口。`AutoTD` 的后台 daemon、自动挂后台、自动刷新/刷取、PID 管理、Web 管理端和 telemetry 明确排除。Cloud mount 只按“协议/VFS 已有、adapter 接入状态、外部依赖、真实系统挂载验证”分层记录，未完成 adapter 不得写成已可用挂载能力。

## 对比结果

| 参考源 | 参考能力 | UBAANext 当前状态 | 本轮处理 |
| --- | --- | --- | --- |
| `reference/buaa-api` | 课堂直播周课表、AAS/Cloud/SPOC/Signin/SRS/Evaluation/WiFi 等 typed API | 已有 `live week`、教务/云盘/作业/签到等入口 | 保持 `live week` 课表语义；新增课堂资源下载不放入 `course`，避免混淆教务课表 |
| `reference/UBAA` | 原 UBAA 桌面/业务入口与用户操作习惯 | CLI 已有统一命令树和 JSON envelope | 继续用 `live` 作为课堂资源入口，输出仍用 `FeatureRecord`，便于 GUI/脚本复用 |
| `autotd-buaa 0.1.15` | TD 配置、用户、图片、TCP 协议、自动调度/后台辅助 | 已有 TD config/user/store/protocol/scheduler/run once；无后台 daemon | 新增 `td image delete <name> --confirm [--force]`；删除前校验安全文件名和用户引用 |
| `reference/BBUAA` | 按课堂资源搜索、livingroom 播放页、PPT 时间轴、PPT 图片、视频回放/HLS | 旧版只有 `live week` | 新增 `LiveService resources/detail/ppt_slides/prepare_download/download_binary`、CLI `live resources/detail/download`，并为 Harmony 迁移暴露 C ABI/NAPI `liveWeek/liveResources/liveDetail` |

## 已补齐功能

| 功能 | Core | CLI | 测试 |
| --- | --- | --- | --- |
| 按日期搜索课堂资源 | `Model::LiveResourceQuery`、`LiveService::resources` 使用 classroom 搜索入口；有 `_token` 时附加 Bearer | `ubaa live resources --date <yyyy-MM-dd>` | parser 状态映射、CLI mock JSON、真实 VPN resources |
| 课堂资源详情解析 | `LiveResourceDetail`、`parse_live_resource_detail`、`parse_live_livingroom_html`；yjapi 详情优先 Bearer，livingroom 降级 | `ubaa live detail --course-id <id> --sub-id <id>` | sub_content/video_list/livingroom HTML 单元测试、真实 VPN detail |
| PPT 时间轴与 PPTX 生成 | `LivePptSlide`、`parse_live_ppt_slides`、`build_live_pptx` | `ubaa live download --include ppt` | PPTX ZIP/OOXML 结构测试，CLI mock 实际生成 PPTX |
| 视频下载与 HLS fallback | `LiveDownloadResult`、`prepare_download` | MP4 直接写文件；HLS 尝试 `ffmpeg`，失败写 `.m3u8.url` | CLI mock 下载、overwrite 拒绝/允许 |
| 与课表合并入口 | `LiveResourceQuery::from_course` | `--from-course` 对当天教务课表做保守过滤 | 真实 smoke 只读路径覆盖 |
| TD 图片删除 | `TdStore::delete_image` | `td image delete <name> --confirm [--force]`；C ABI/NAPI `tdImageDelete({ name, force, confirmed })` 供 Harmony 本地管理图片 | 不存在、被引用、force、路径穿越单元/集成测试；C ABI smoke 覆盖未确认和缺失文件 |

## 明确排除

- BBUAA Web 播放器复刻、视频代理服务、直播缓存、录制服务。
- AutoTD 后台 daemon、自动挂后台、后台 PID 管理、自动刷新/刷取、无人值守刷取控制 UI。
- 课堂资源下载的默认真实请求。默认 `ctest` 仅跑 mock/offline；真实搜索、详情和下载必须通过 `tools/live-smoke.ps1` 显式开启。

## 真实性测试边界

- 默认验证：`cmake --build --preset windows-ninja-msvc-debug` 与 `ctest --test-dir build/windows-ninja-msvc-debug --output-on-failure`，不访问真实课堂资源后端。
- 真实只读验证：`.env` 提供凭据后，VPN 模式 `login`、`whoami`、`term list`、`live week` 和多数只读域可执行；本轮隔离 app data 复测中 `live resources` 未成功，失败原因为 WebVPN 会话只出现 `d.buaa.edu.cn` 网关 cookie，未获得 classroom `_token/JWTUser/login_cmc_id`，因此无法生成 Bearer token。direct 模式 `live resources` 触发 TLS 握手失败。真实下载仍需额外开启 `UBAANEXT_LIVE_DOWNLOAD=1`，且依赖 `live resources/detail` 成功。
- 显式真实 smoke：设置 `UBAANEXT_LIVE=1`、真实凭据和 `UBAANEXT_APP_DATA_DIR` 后运行 `tools/live-smoke.ps1 -Level L1`。
- 课堂资源真实下载：额外设置 `UBAANEXT_LIVE_DOWNLOAD=1`；建议先用 `UBAANEXT_LIVE_DOWNLOAD_INCLUDE=ppt`，需要视频时再改为 `ppt,video`。
- 报告未成功项时需区分：缺失凭据、校园网不可达、资源为空、PPT 无 GUID、图片下载失败、视频为 HLS 且无 `ffmpeg`、只生成 sidecar、临时目录清理失败。
