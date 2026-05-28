# 原 UBAA 非 UI 后端差异报告

> 当前仓库版本阶段为 `v0.4.0`。本报告是早前提交 `83b2626` 的历史差异快照，记录 v0.5+/Harmony 非 UI 接入方向的阶段性审计结果；其中 C ABI、真实协议和 Harmony 对接内容不代表 v0.4 当前稳定承诺。

## 结论

截至提交 `83b2626`，`D:\Code\Cpp\UBAANext` 的 C++ core 已经完成本轮 Harmony 非 UI 接入所需的主要代码对齐：稳定 C ABI 已从 version/capabilities smoke 扩展到 context、session、Auth、Term、Course、Grade、Exam、Todo、Signin、YGDK 等核心入口；Signin loginName 跳转解析、WebVPN URL 反解、写操作门控、错误脱敏、Direct/WebVPN/Mock 运行时分桶和 C bindings 构建导出均已落地。

当前项目不是“所有线上业务都已经真实验证完成”，但就本仓库本轮目标而言，剩余未完成部分已经收敛为两类：

1. **live 真实性验证**：需要用户显式提供安全凭据并设置 `UBAANEXT_LIVE=1`、`UBAANEXT_USERNAME`、`UBAANEXT_PASSWORD` 后，按 L1 只读 smoke 分 direct/WebVPN 跑真实远端验证；默认环境不能也不应执行真实登录。
2. **Harmony 工程闭环**：需要切到 `harmony/ubaanext` 或 `D:\Code\OpenHarmony\UBAANext` 工程，重新链接当前 C ABI，完成 HAP native `.so` 打包、NAPI 调用、ArkTS 错误处理和真机/模拟器加载验证。

因此，本 C++ 仓库目前不再因为“缺少 C ABI 业务入口”阻塞 Harmony UI；但真实账号验证、真机端到端和写操作专项仍不能被标记为完成。

## 本轮已提交内容

提交：`83b2626 对齐 Harmony 可调用的非 UI 核心能力`

主要代码变化：

- 新增 `bindings/c` 稳定 C ABI target `UBAANextBindingsC`，构建产物为 `ubaanext_c`。
- 新增 `UbaaNextContext` 生命周期、连接模式切换和统一结果释放 `ubaanext_release_result`。
- C ABI 输出统一 JSON envelope：`ok`、`data`、`error`。
- C ABI 暴露：
  - `ubaanext_auth_login`
  - `ubaanext_auth_logout`
  - `ubaanext_auth_restore_session`
  - `ubaanext_auth_get_session_state`
  - `ubaanext_terms`
  - `ubaanext_weeks`
  - `ubaanext_courses_today`
  - `ubaanext_courses_date`
  - `ubaanext_courses_week`
  - `ubaanext_grades`
  - `ubaanext_exams`
  - `ubaanext_todos`
  - `ubaanext_signin_today`
  - `ubaanext_signin_do`
  - `ubaanext_ygdk_overview`
  - `ubaanext_ygdk_records`
- C ABI 复用 core service 和 platform adapter，不依赖 CLI private 头。
- C ABI context 内按 Direct/WebVPN/Mock 分桶持有 volatile secure store、memory cache 和 curl network stack，降低模式串用 session/cookie/cache 的风险。
- `SigninService` 对齐新版 UBAA iClass loginName 流程：访问 8346 jumpMyCenter，禁用传输层自动重定向，手动跟随最多 8 次，从 URL 或 Location 中大小写无关解析 `loginName`，支持 percent decode，再用 loginName 登录 8347。
- `SigninService` 写操作接入 `WriteOperationGate`，默认 fail-closed，必须显式 confirm。
- `VpnCipher` 新增 `from_vpn_url`，支持 WebVPN URL 反解并保留端口、query、fragment。
- 抽出共享 `SecurityRedaction`，统一 CLI、core service、downstream diagnostics 和 C ABI 错误边界脱敏。
- 补充 `WriteOperationGate`、`TimeUtils`、`HttpHeaders` 等公共基础设施。
- 更新 CMake package/export/install 配置，使 C bindings 可随 SDK 输出。
- 快进 `reference/UBAA`，以最新版参考仓库作为非 UI 行为契约来源。

## 已验证结果

本轮提交后重新执行并通过：

- `cmake --build build/windows-ninja-msvc-debug --target UBAANextBindingsC`
- `cmake --build build/windows-ninja-msvc-debug`
- `ctest --test-dir build/windows-ninja-msvc-debug --output-on-failure`

测试结果：

- `294/294` 通过。
- 总测试时间约 `33.61 sec`。

C ABI 动态库导出检查通过，`ubaanext_c.dll` 当前导出 22 个 `ubaanext_*` 符号：

- `ubaanext_auth_get_session_state`
- `ubaanext_auth_login`
- `ubaanext_auth_logout`
- `ubaanext_auth_restore_session`
- `ubaanext_context_create`
- `ubaanext_context_release`
- `ubaanext_context_set_connection_mode`
- `ubaanext_courses_date`
- `ubaanext_courses_today`
- `ubaanext_courses_week`
- `ubaanext_exams`
- `ubaanext_get_capabilities`
- `ubaanext_grades`
- `ubaanext_release_result`
- `ubaanext_signin_do`
- `ubaanext_signin_today`
- `ubaanext_terms`
- `ubaanext_todos`
- `ubaanext_version`
- `ubaanext_weeks`
- `ubaanext_ygdk_overview`
- `ubaanext_ygdk_records`

live smoke 执行情况：

- 默认执行 `./tools/live-smoke.ps1 -Level L1` 时安全跳过：需要 `UBAANEXT_LIVE=1`。
- 显式设置 `UBAANEXT_LIVE=1` 后，脚本在凭据门禁处停止：需要 `UBAANEXT_USERNAME` 和 `UBAANEXT_PASSWORD`。
- 当前环境未提供真实凭据，因此没有执行真实登录或远端业务请求。
- 该结果符合安全预期：真实性测试必须由用户显式开启并提供凭据，不能默认触发远端登录。

## 安全性审查

### 已满足的安全边界

| 边界 | 当前状态 | 说明 |
| --- | --- | --- |
| 敏感输入脱敏 | 已落实 | C ABI 错误、CLI 输出、downstream diagnostics 进入用户可见边界前走共享 `SecurityRedaction`。 |
| C ABI 账号输出 | 已收窄 | C ABI `account` 输出只包含 `studentId` 和 `displayName`，不返回 `access_token`、`refresh_token`。 |
| 写操作默认拒绝 | 已落实 | `SigninService::perform_signin` 通过 `WriteOperationGate`；未 confirm 或平台未开启写能力时 fail-closed。 |
| live smoke 默认关闭 | 已落实 | 未设置 `UBAANEXT_LIVE=1` 时直接跳过。 |
| live smoke 凭据门禁 | 已落实 | 开启 live 后仍要求 `UBAANEXT_USERNAME` 和 `UBAANEXT_PASSWORD`。 |
| live smoke 输出脱敏 | 已落实 | runner 对命令输出执行环境变量值、URL query、Cookie、Authorization、本地路径、移动端路径、HTML 等脱敏。 |
| C ABI 空 context | 已处理 | 空 context 返回稳定 `InvalidArgument` JSON 错误。 |
| C ABI 结果释放 | 已处理 | 调用方必须用 `ubaanext_release_result` 释放由 C++ 分配的 JSON 字符串。 |
| Direct/WebVPN/Mock 分桶 | 已处理 | C ABI context 内不同 mode 使用不同 store/cache/network bucket，避免同一 context 内跨模式串 session。 |

### 仍需关注的安全风险

| 风险 | 当前处置 | 后续要求 |
| --- | --- | --- |
| 真实账号凭据 | 本仓库不保存、不提交、不默认读取。 | 用户运行 live 时只能通过环境变量注入，并确认日志脱敏后再保存输出。 |
| 写操作业务风险 | 仅 Signin C ABI 暴露写入口且受 confirm gate；其他写 UI 暂不接入。 | Harmony 侧不得绕过 `WriteOperationGate`；任何真实写 UI 都需要独立安全设计。 |
| 成绩、锁码、预约、打卡记录等高敏感输出 | core/CLI 已加强脱敏，但业务数据本身仍敏感。 | UI 和日志层必须避免持久化明细；真机调试日志需单独审查。 |
| C ABI volatile store | 用于 Harmony bridge 的默认 runtime，避免 unsupported secure store 写崩溃。 | 这不是长期持久化 secure store；真实 Harmony 登录前仍需确认平台 secure store/cookie persistence 方案。 |
| result 指针生命周期 | ABI 已提供 release 函数。 | NAPI 层必须保证每次非空 result 都释放，不能跨 allocator free。 |
| 异常消息 | `std::exception::what()` 会进入脱敏链路。 | 后续新增异常必须避免把原始请求体、HTML、URL query 或凭据放入 exception message。 |

## 跨平台审查

### 已满足的跨平台边界

- core 仍保持平台无关，真实网络、OpenSSL、secure store、app data path 等由 platform target 承担。
- C ABI 根据 `_WIN32`、`__OHOS__`、`__linux__` 选择对应 `PlatformCapabilities`，未知平台回落为默认能力结构。
- C bindings 通过 CMake target 链接 `UBAANextCore`、`UBAANextPlatformCurrent`、`UBAANextPlatformCurl`、`UBAANextPlatformOpenSSL`，避免 include 可见但 link 缺失。
- Windows Debug 下 C bindings 和全量目标已构建通过。
- OpenSSL crypto provider 在 C ABI context 创建时安装，避免 WebVPN cipher 等路径缺 provider。
- C ABI header 使用 `extern "C"` 和平台导出宏，Windows 下使用 `__declspec(dllexport/dllimport)`，非 Windows 下使用 default visibility。

### 仍需在 Harmony 工程验证的跨平台项

| 项目 | 当前状态 | 未完成原因 |
| --- | --- | --- |
| OHOS toolchain 构建 | 未在当前路径完成 | DevEco MCP 当前指向路径不是 Harmony 工程，找不到对应工程结构。 |
| `check_cpp_files` | 未完成 | DevEco MCP 查找 `compile_commands.json` 的路径不正确。 |
| HAP native 依赖闭包 | 未在本仓库验证 | 需要进入 Harmony 工程重新链接 `.so` 并检查 RPATH/RUNPATH/依赖闭包。 |
| NAPI result release | 未在本仓库验证 | 需要 ArkTS/NAPI 包装层调用 C ABI 后确认释放策略。 |
| Harmony secure store / cookie persistence | 未证明 | 当前 C ABI runtime 使用 volatile store；真机持久化能力需要 Harmony 工程接入后验证。 |
| 真机日志脱敏 | 未证明 | 需要在 Harmony 工程和设备 hilog 中确认不会输出敏感字段。 |

## 业务域差异矩阵

| 功能域 | 当前 C++ core 状态 | Harmony 接入判断 | 剩余工作 |
| --- | --- | --- | --- |
| Auth / session | C ABI 已暴露 login/logout/restore/session state；复用 `AuthService`。 | 可接 NAPI，但真实登录 UI 需 secure store/cookie persistence 和 live_login 能力确认。 | live direct/WebVPN 登录验证；Harmony 持久化策略。 |
| Term / Week | C ABI 已暴露 terms/weeks。 | 可优先接只读 UI。 | live 只读验证字段漂移。 |
| Course | C ABI 已暴露 today/date/week。 | 可优先接只读 UI。 | live 只读验证空课表、跨周、节次字段。 |
| Grade | C ABI 已暴露 grades。 | 可接受控只读 UI，注意高敏感输出。 | live 验证错误分类和字段漂移；UI 禁止日志记录成绩明细。 |
| Exam | C ABI 已暴露 exams。 | 可接只读 UI。 | live 验证空考试和时间地点格式漂移。 |
| Todo | C ABI 已暴露 todos。 | 可接只读 UI，但必须展示 partial failure。 | live 验证各来源失败是否可解释。 |
| Signin 今日列表 | C ABI 已暴露 signin_today；loginName 解析已对齐新版 UBAA。 | 可接只读状态 UI。 | live 验证 iClass redirect/loginName 在真实环境下稳定。 |
| Signin 执行 | C ABI 已暴露 signin_do，但受 confirm gate。 | 不进入默认真实 UI；只能做受控写操作。 | 独立写操作安全专题和用户显式授权。 |
| YGDK 概览/记录 | C ABI 已暴露 overview/records。 | 可接只读 UI，注意隐私展示。 | live 验证记录字段和体育分类选择。 |
| SPOC / Judge / BYKC / Evaluation / Classroom / Library / Venue | core/CLI/service 层已有不同程度对齐，但本轮 C ABI 尚未全部暴露。 | Harmony 如果需要这些页面，下一阶段应继续扩展 C ABI 或 NAPI typed wrapper。 | 不是本轮 exit criteria 的阻塞项，但属于“全量非 UI”继续扩展范围。 |
| 文件/附件上传 | CLI 占位稳定失败；YGDK photo 属写操作场景。 | 不接真实 UI。 | typed API、隐私策略、文件校验和写门控专项。 |

## 当前未完成部分

### 本 C++ 仓库剩余

1. **live L1 真实性测试未运行成功**
   - 原因：当前环境没有 `UBAANEXT_USERNAME` 和 `UBAANEXT_PASSWORD`。
   - 需要用户提供安全凭据或在本机 shell 中设置环境变量。
   - 应先跑只读 L1，不跑写操作。

2. **direct 与 WebVPN 两种真实路径都需要分别验证**
   - direct 可能依赖校园网或 VPN 网络环境。
   - WebVPN 需要验证 CAS、cookie、redirect、VpnCipher 和下游系统激活链路。

3. **部分业务域尚未通过 C ABI 暴露**
   - 本轮满足 Auth、Course、Grade、Exam、Todo、Signin、YGDK 等 Harmony UI 解除 pending 的主链路。
   - 如果严格要求原 UBAA 所有非 UI 页面立即由 C ABI 覆盖，SPOC、Judge、BYKC、Evaluation、Classroom、LibrarySeat、VenueReservation 等还需要继续扩展 ABI。

4. **真实写操作未验证，也不应默认验证**
   - Signin 执行已接入 gate，但真实写需要单独授权。
   - 博雅选课/退课、场馆预约/取消、图书馆预约/取消、评教提交、阳光打卡提交/图片上传等仍必须保持写门控和默认关闭。

5. **生成/本地目录未提交**
   - 工作树仅剩 `.antigravitycli/`、`vcpkg-manifest-install.log`、`vcpkg_installed/` 未提交。
   - 这些是本地工具或依赖生成内容，不应进入 Git。

### Harmony 工程剩余

1. 重新链接当前 `ubaanext_c` C ABI。
2. 在 NAPI 层包装 context 生命周期、result release、JSON envelope 和错误码映射。
3. 将 ArkTS pending 状态替换为真实 C ABI 调用结果或真实错误展示。
4. 验证 HAP native `.so` 打包和依赖闭包。
5. 验证真机/模拟器 `.so` 加载、version/capability、Auth/Course/Grade/Exam/Todo/Signin/YGDK 调用链。
6. 验证 hilog、ArkTS exception、NAPI diagnostics 不泄露 username/password/captcha/token/cookie/photo path/location 等敏感信息。
7. 真实登录 UI 前完成 secure store、cookie persistence、session restore 和 live_login capability 语义确认。
8. 真实写 UI 继续不接入，直到独立写操作安全专题完成。

## live 真实性测试执行建议

在用户明确授权并设置凭据后，建议按以下顺序执行：

1. direct L1 只读：
   - `UBAANEXT_LIVE=1`
   - `UBAANEXT_CONNECTION_MODE=direct`
   - `UBAANEXT_USERNAME=...`
   - `UBAANEXT_PASSWORD=...`
   - `./tools/live-smoke.ps1 -Level L1`

2. WebVPN L1 只读：
   - `UBAANEXT_LIVE=1`
   - `UBAANEXT_CONNECTION_MODE=webvpn`
   - `UBAANEXT_USERNAME=...`
   - `UBAANEXT_PASSWORD=...`
   - `./tools/live-smoke.ps1 -Level L1`

3. 如需要指定学期/周次：
   - 设置 `UBAANEXT_TERM`。
   - 可选设置 `UBAANEXT_WEEK`。

4. 不执行 L2/L3 写操作，除非用户专门授权具体业务动作。

运行后只保存脱敏结果，不保存原始账号、密码、cookie、token、ticket、session、HTML、URL query、本地路径或照片路径。

## Harmony 接入建议

下一步应转到 Harmony 工程做以下工作：

1. 确认工程路径，例如 `D:\Code\OpenHarmony\UBAANext` 或用户所说的 `harmony/ubaanext`。
2. 更新 native 构建配置，使其链接本仓库提交后的 `UBAANextBindingsC` / `ubaanext_c`。
3. NAPI 第一层只做稳定桥接：
   - context create/release。
   - set connection mode。
   - release result。
   - version/capabilities。
   - Auth/Course/Grade/Exam/Todo/Signin/YGDK 只读入口。
4. ArkTS 层保留 JSON envelope，不把 C++ 错误折叠成 pending 或空数据。
5. 先跑离线/无凭据 smoke，再跑真机 capability 和 `.so` 加载。
6. live 登录和真实只读业务必须继续受用户凭据授权控制。
7. 写操作 UI 不进入默认接入范围。

## 最终判断

本轮 C++ core 侧的代码对齐、C ABI 暴露、安全边界和离线质量门禁已经完成；当前项目剩余的核心未完成项确实主要是 **真实性测试**。不过“真实性测试”包含两层：

- C++ 仓库自身的 direct/WebVPN L1 live 只读验证。
- Harmony 工程中的 native/NAPI/HAP/真机端到端验证。

在没有真实凭据和 Harmony 工程重链接前，不能宣称整个产品业务闭环完成；但可以进入 `harmony/ubaanext` 继续对接。