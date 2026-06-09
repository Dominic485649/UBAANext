# buaa-api 迁移矩阵

> 基准来源：`reference/buaa-api` Rust crate，已在迁移前执行 `git pull --ff-only`，结果为 `Already up to date`。本矩阵只覆盖 `buaa-api` crate，不扩大到 `reference/UBAA` 或 `reference/AutoTD`。

## 总览

| buaa-api 模块 | UBAANext Core | CLI 入口 | 状态 | 测试覆盖 | 真实性测试边界 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| SSO/Auth | `AuthService`、`SessionManager`、`RedirectNavigator`、`CookieJar` | `login`、`relogin`、`logout`、`whoami` | 已迁移并增强 | `AuthServiceTests`、`SessionManagerTests`、CLI integration | L1 可用 `.env` 登录/用户信息；登出属于本地/远端状态写，需确认 |
| AAS/App 教务 | `TermService`、`CourseService`、`ExamService`、`GradeService` | `term/week/course/exam/grade` | 已迁移并保留真实只读入口 | 对应 service/parser tests、CLI smoke | L1 只读；`course week` 支持显式 term，真实模式可用当前配置 |
| Class | `ClassroomService` | `classroom query` | 已迁移 | `ClassroomServiceTests`、CLI tests | L1 只读 |
| Live | `LiveService` | `live week/resources/detail/download` | 已迁移并按 BBUAA 补齐课堂资源下载 | `LiveServiceTests`、CLI tests | L1 搜索/详情只读；下载需 `UBAANEXT_LIVE_DOWNLOAD=1` |
| Boya/BYKC | `BykcService` | `bykc profile/courses/chosen/stats/course show/select/unselect/sign` | 已迁移并增强 | `BykcParserTests`、`BykcServiceTests`、CLI write-gate tests | 只读可 smoke；选课/退选/签到不自动真实执行。`sign` 必须显式 `--lat/--lng`，不会默认伪造位置 |
| Smart Class/Signin | `SigninService` | `signin today/schedule/courses/course schedule/do` | 已迁移并增强 | `SigninServiceTests`、CLI tests | 只读可 smoke；真实签到不自动执行 |
| Cloud | `CloudService`、`IUploadSource`、`CloudVfs` | `file roots/root/list/size/recycle/shares/suggest-name/mkdir/rename/move/copy/delete/recycle-delete/recycle-restore/share-* /download-url/batch-download-url/upload`；桌面 UI 有 Cloud 浏览和 mount 按钮 | CLI/协议与只读 VFS 已迁移并增强到读写/上传；系统挂载 adapter 仍进行中 | `CloudServiceTests`、CLI upload/write-gate tests、Cloud VFS 单元路径 | L1 只读；可逆写 smoke 只允许临时目录/小文件/分享/删除回收站清理，并需显式开关；WinFsp/Cloud Files/FUSE 实际挂载需 adapter 注册、外部依赖和人工真实验证 |
| SPOC | `SpocService` | `spoc week/schedule/courses/assignments/assignment show/homework submit` | 已迁移并增强 | `SpocParserTests`、`SpocServiceTests`、CLI tests | 只读可 smoke；作业提交不自动真实执行 |
| Judge | `JudgeService` | `judge assignments/assignment show/details/details-batch` | 已有并对齐待办场景 | `JudgeParserTests`、`JudgeServiceTests`、CLI tests | L1 只读 |
| SRS | `SrsService` | `srs config/batch/course query/preselected/selected/course preselect/select/drop` | 已迁移 | `SrsServiceTests`、CLI write-gate tests | 选课/退选不自动真实执行 |
| TES/Evaluation | `EvaluationService`、`EvaluationForm` | `evaluation list/form/submit/form submit` | 已迁移并增强表单读取/指定提交 | `EvaluationParserTests`、`EvaluationServiceTests`、CLI tests | 任务/表单可只读；真实提交不自动执行 |
| User | `AuthService::current_account` / Feature record | `user info`、`whoami` | 已对齐 | CLI tests | L1 只读 |
| WiFi | `WifiService`、`INetworkEnvironment`、`ProtocolCrypto` | `wifi login/logout` | 已迁移并 fail-closed | `WifiServiceTests`、`ProtocolCrypto` tests、CLI write-gate tests | 不自动真实执行；需要校园网环境、本机 IPv4、显式确认 |

## 跨平台边界

- Core 不包含 Win32、Linux/POSIX、Curl、OpenSSL 或路径策略头；平台差异通过 `IHttpClient`、Cookie/SecureStore/Cache/Crypto、`IUploadSource`、`INetworkEnvironment` 注入。
- 云盘上传由 CLI/Platform 读取本地文件并实现 `IUploadSource`，Core 只消费流式字节源。
- WiFi 校园网检测由 Windows/Linux platform adapter 实现；未知平台使用 fail-closed 环境。
- 远端写操作统一使用 `WriteOperationGate`，CLI 的 `--json` 模式缺少 `--confirm|--yes|-y` 必须 fail-closed。

## 已验证

- Windows Debug 构建通过：`UBAANextTests`、`UBAANextCliTests`。
- 目标测试已覆盖 Cloud、SRS、SPOC、Signin、WiFi、BYKC、Evaluation 的新增 service/parser/CLI 写门控路径。
- 全量 `ctest` 基线应保持默认 mock/offline，不访问真实公告或课堂资源后端。

## 未自动执行的真实写测

以下操作会改变真实校园系统状态，不纳入默认自动 smoke：SRS 选课/退选、BYKC 选课/退选/签到/签退、课程签到、评教提交、SPOC 作业提交、WiFi 登录/登出、阳光打卡、场馆预约/取消、图书馆座位预约/取消。Cloud 可逆写 smoke 只在用户显式开启并允许清理临时数据时运行。
