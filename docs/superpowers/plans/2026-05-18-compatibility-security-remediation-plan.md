兼容性与安全性整改计划
本计划只基于当前只读分析结果，不包含代码实现步骤；本次不修改文件、不构建、不测试、不安装。

1. 整改总目标
目标是把当前“C++ Core + Windows CLI 为主”的实现，收敛到真正可跨 Windows / HarmonyOS / Linux 演进的结构：

Core
  -> Interface
    -> Platform Adapter
      -> Windows
      -> HarmonyOS
      -> Linux
      -> Mock
核心原则：

Core 只依赖接口，不依赖 WinHttpClient、DPAPI、BCrypt、OHOS SDK、CLI 文件路径等平台细节。
Windows、HarmonyOS、Linux、Mock 各自提供 Platform Adapter。
不安全 fallback 必须显式禁用或显式标记，不能静默退化为明文、空实现或 nullptr。
HarmonyOS ArkUI / NAPI 作为平台 Shell，不直接绑定 Core 内部实现类。
Linux 先以 mock-only 可构建为最低目标，再补真实网络、加密、安全存储。
2. 兼容性问题分级
P0：必须先修，否则跨平台架构无法成立
问题 当前证据 影响 整改方向
Core 直接依赖 WinHttpClient core/CMakeLists.txt:54-56 在 Core target 内按 WIN32 编译 WinHttpClient；core/CMakeLists.txt:66-70 在 Core 内链接 winhttp/crypt32/bcrypt/libohcrypto.so Core 不再是平台无关库；HarmonyOS/Linux 会被平台分支污染 将 WinHttpClient 移出 core，放入 platform/windows；Core 只保留 IHttpClient 等接口
公共 Core 伞形头暴露 WinHttpClient UBAANext.hpp:39-41 在 _WIN32 下 include WinHttpClient 下游包含 UBAANext.hpp 时会看到 Windows 具体实现，破坏公共 API 边界 公共伞形头只导出接口和模型；平台实现由 platform/windows 头单独导出
VenueReservationService dynamic_cast 到 WinHttpClient VenueReservationService.cpp:7-9 include WinHttpClient；VenueReservationService.cpp:175-190 dynamic_cast<WinHttpClient*> 读取 CookieJar Core Service 依赖 Windows CookieJar 内部能力，HarmonyOS/Linux 无法实现真实 CGYY 流程 抽象 ICookieStore / ISessionCookieProvider / IRedirectController，VenueReservationService 只依赖接口
CookieJar / 重定向能力未抽象 WinHttpClient 暴露 cookies()、save_cookies/load_cookies、RedirectGuard/scoped_redirects；VenueReservationService 直接依赖 WinHttpClient cookie 读取能力 Cookie、跳转、SSO token 获取逻辑无法跨平台复用 从 IHttpClient 拆出 ICookieStore 和 IRedirectController，或引入 INetworkStack 聚合 HTTP + Cookie + Redirect
WinHTTP 真实网络只支持 Windows WinHttpClient.hpp 明确非 _WIN32 #error，包含 windows.h/winhttp.h；main.cpp:542-553 非 Windows ctx.http = nullptr HarmonyOS/Linux 无真实网络能力 新增 Harmony/Linux HTTP adapter；在未实现真实 adapter 时，真实登录/真实 API 必须返回明确 UnsupportedPlatform
Core 目标直接链接平台加密库 core/CMakeLists.txt:66-70 Windows 链接 bcrypt/crypt32，OHOS 链接 libohcrypto.so Core target 本身变成平台 target，不利于通用 Linux 构建和 ABI 稳定 Core 只定义 ICryptoProvider；平台库链接移动到 platform/windows、platform/harmony target
P1：应尽快修，否则 HarmonyOS/Linux 无法真实运行
问题 当前证据 影响 整改方向
CryptoProvider Windows / HarmonyOS / Linux 实现不一致 Windows BCrypt/CryptoAPI 分支：CryptoProvider.cpp:44-168；OH Crypto 分支：CryptoProvider.cpp:168-336；fallback NotImplemented：CryptoProvider.cpp:338-370 Linux 真实功能不可用；HarmonyOS 依赖 OHOS SDK 且未确认运行 将 CryptoProvider 变成平台 adapter；Linux 明确选用平台 API 或统一外部库；未实现平台禁止真实登录
SecureStore 在 HarmonyOS/Linux 缺安全实现 main.cpp:556-560 Windows encrypted=true，非 Windows encrypted=false；PlainFileStore.cpp:139-147 非 Windows encrypted=true 时直接 return HarmonyOS/Linux session/token 可能明文或无法安全保存 Windows 用 DPAPI；HarmonyOS 用 HUKS/系统安全存储；Linux 用 Secret Service/libsecret，或明确禁用真实登录
Linux preset 缺失 linux-build-plan.md:1-3 仍 TODO；CMakePresets.json 仅 Windows/OpenHarmony preset Linux 可移植性无法持续验证 增加 linux-ninja-gcc-debug / linux-ninja-clang-debug，先 mock-only
HarmonyOS ArkUI / Hvigor / NAPI 工程缺失 harmony-arkui.md:1-3、napi-api-plan.md:1-3、binding-design.md:1-7 均为 TODO；未发现*.ets/hvigorfile.ts/oh-package.json5/module.json5 HarmonyOS 只能 Native 编译预研，不能形成 App 先定义 C API/NAPI 边界，再建 apps/harmony 工程
OpenHarmony preset 复用 Windows vcpkg include 路径 CMakePresets.json:107-112 关闭 FETCH_DEPS 并指向 build/windows-ninja-msvc-debug/vcpkg_installed/x64-windows/include；harmony-native-build-plan.md:46-54 说明复用 Windows vcpkg 头文件 OH 构建依赖 Windows 产物路径，移植性差 改为独立 third_party/include、OH 专用依赖缓存或可配置环境变量
CMakePresets 硬编码本机绝对路径 CMakePresets.json:18-28、CMakePresets.json:103-112 写死 VS/Windows Kits/DevEco 路径 新机器、CI、Linux/Harmony 构建不可复现 用环境变量、CMakeUserPresets、本地 preset include 或文档化覆盖
P2：可以后续优化
问题 当前证据 影响 整改方向
文档与实现漂移 架构文档宣称 Core 无外部依赖：module-boundaries.md:22-24，但 Core 实际 PUBLIC 链接 nlohmann_json 并链接平台库：core/CMakeLists.txt:58-70 新贡献者会误判边界 更新 ARCHITECTURE / PORTING / BUILD
live-smoke 偏 Windows CLI 路径 live-smoke.ps1:1-6 默认 .\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe Linux/Harmony 验证入口不统一 拆分用例定义与平台 runner
nlohmann-json 作为 Core PUBLIC 依赖 core/CMakeLists.txt:58-60 Header ABI/编译依赖外泄 后续可评估是否把 JSON 依赖隐藏在 Parser 实现中
C++20/OH 构建验证与测试缺口 CMakePresets.json:116-130 有 C++20 OH preset，但 CMakePresets.json:107 关闭测试 兼容性回归难发现 后续补平台 smoke / mock-only test preset
3. 安全性问题分级
P0：可能导致凭据、Cookie、Token、Session 泄漏
问题 当前证据 风险 整改方向
.env 可能存在敏感信息 顶层存在 .env；.gitignore:38-40 忽略 .env/.env.local；本次未读取 .env 内容 如果误提交或日志输出，可能泄漏账号、密码、token 保持 .env 不纳入版本；增加 secret scan；文档明确 .env.example 只放占位符
Cookie / Session 持久化文件存在泄漏面 main.cpp:508-517 使用 session.dat/cookies.dat/config.json；main.cpp:876-884 Windows 保存真实 cookies cookies.dat/session.dat 若明文或权限不当，可能泄漏登录态 Cookie/Session 必须进入平台安全存储或加密 CookieStore；权限与路径策略进入 Platform Adapter
非 Windows encrypted=false main.cpp:556-560 非 Windows PlainFileStore encrypted=false HarmonyOS/Linux 若启用真实登录，会明文保存 session/token 非 Windows 在安全存储未实现前禁止真实登录，或只允许 mock-only
DPAPI 只适用于 Windows PlainFileStore.cpp:44-73 使用 CryptProtectData/CryptUnprotectData 安全能力不可迁移；非 Windows 没有等价保护 WindowsDPAPISecureStore 仅放 platform/windows；Harmony/Linux 各自实现
日志/输出可能泄漏敏感信息 live-smoke.ps1:10-19 有 Redact-Text；live-smoke.ps1:22-28 会输出命令和输出 已有脚本脱敏，但应用内部 OutputFormatter/错误输出是否全链路脱敏未确认 把 SecurityRedaction 作为统一输出边界；审计 CLI JSON/错误输出/异常消息
P1：加密实现不一致或平台 fallback 不安全
问题 当前证据 风险 整改方向
Linux Crypto fallback 为 NotImplemented CryptoProvider.cpp:338-370 Linux 真实登录/签名/加密流程不可用；如果调用方忽略错误会失败 Linux 明确选择 Secret Service + OpenSSL/BoringSSL/平台 API，未实现前真实模式 fail-fast
HarmonyOS Crypto 依赖 OH Crypto API CryptoProvider.cpp:22-24、CryptoProvider.cpp:168-336 SDK/ABI/算法名称兼容性需要验证 HarmonyCryptoProvider 移到 platform/harmony，并用 DevEco 构建/设备 smoke 验收
PlainFileStore 非 Windows encrypted=true 直接不保存 PlainFileStore.cpp:139-147 行为可能被误解为“已加密保存”，实际可能丢数据或未保存 不允许 PlainFileStore 表示安全存储；命名拆分 PlainFileStore 和 SecureStore
Crypto 错误码使用 NetworkError CryptoProvider.cpp:66-75 多处加密失败返回 NetworkError 安全审计与错误分类不清 增加 CryptoError 或 PlatformCryptoError
P2：合规、许可证、文档和审计问题
问题 当前证据 风险 整改方向
third-party notices 不完整 third-party-notices.md:1-3 仍 TODO；vcpkg.json:4 有 nlohmann-json/catch2 发布合规风险 补 DEPENDENCIES.md / THIRD_PARTY_NOTICES
架构安全边界文档不足 overall-architecture.md:38-44 只有原则，缺安全边界 平台实现容易继续污染 Core 增加 SECURITY_ARCHITECTURE / PORTING
live smoke 涉及真实账号环境变量 live-smoke.ps1:36-38 要求 UBAANEXT_USERNAME/PASSWORD 本地/CI 环境变量治理需要文档 明确 live-smoke 只本地运行，CI 禁止真实凭据
4. 目标架构建议
目标依赖方向

apps/cli
apps/harmony
tests
  -> platform/{windows,harmony,linux,mock}
    -> core/include interfaces
      -> core/src business logic
Core 内部只允许：

Core
  -> Base / Model / Parser / Protocol / Service
  -> Interface:
       IHttpClient
       ICookieStore
       IRedirectController
       ISecureStore
       ICryptoProvider
       IAppDataPathProvider
       IPlatformCapabilities
       INetworkStack
Platform Adapter 提供：

platform/windows
  -> WinHttpClient
  -> WindowsCookieStore
  -> WindowsRedirectController
  -> DpapiSecureStore
  -> WindowsCryptoProvider
  -> WindowsAppDataPathProvider

platform/harmony
  -> HarmonyHttpClient 或 HarmonyNetworkBridge
  -> HarmonyCookieStore
  -> HarmonySecureStore(HUKS/系统安全存储)
  -> HarmonyCryptoProvider(OH Crypto)
  -> HarmonyAppDataPathProvider
  -> NAPI bridge

platform/linux
  -> LinuxHttpClient
  -> LinuxCookieStore
  -> LinuxSecureStore(Secret Service/libsecret)
  -> LinuxCryptoProvider
  -> XdgAppDataPathProvider

platform/mock
  -> MockHttpClient
  -> MockCookieStore
  -> MockSecureStore
  -> MockCryptoProvider 可选
建议新增或调整接口
接口 目的 为什么需要
IHttpClient 保留发送 HTTP 请求的基础能力 已存在，证据：HttpClient.hpp:53-83
ICookieStore 管理 Cookie 的增删查、序列化、按 host/name 获取 替代 VenueReservationService 对 WinHttpClient::cookies() 的依赖
IRedirectController 控制是否自动重定向、临时关闭/恢复重定向 替代 WinHttpClient::RedirectGuard/scoped_redirects 平台私有能力
INetworkStack 聚合 IHttpClient + ICookieStore + IRedirectController 避免 Service 需要多个平台对象，适合 CGYY/SSO 流程
ISecureStore 保留安全键值存储接口，但实现移出 CLI/Core 平台细节 已存在，证据：SecureStore.hpp
ICryptoProvider 保留加密抽象，但平台实现移出 Core target 已存在，证据：CryptoProvider.hpp
IAppDataPathProvider 提供 session/cookie/config/cache 路径 替代 main.cpp:470-517 中平台路径分支
IPlatformCapabilities 显式暴露 supportsRealNetwork、supportsSecureStore、supportsCrypto、supportsCookiePersistence 避免非 Windows ctx.http=null 或静默 fallback
ISensitiveDataRedactor 统一 CLI、日志、smoke 输出脱敏 对齐 live-smoke.ps1:10-19 已有脚本脱敏逻辑
5. 建议目录结构
建议目标结构：

core/
  include/UBAANext/
    Base/
    Model/
    Parser/
    Protocol/
    Service/
    Interface/
      IHttpClient.hpp
      ICookieStore.hpp
      IRedirectController.hpp
      INetworkStack.hpp
      ISecureStore.hpp
      ICryptoProvider.hpp
      IAppDataPathProvider.hpp
      IPlatformCapabilities.hpp
  src/
    Base/
    Parser/
    Protocol/
    Service/
    Storage/
    Crypto/          # 仅保留抽象/无平台实现

platform/
  windows/
    include/
    src/
      WinHttpClient.cpp
      WindowsCookieStore.cpp
      DpapiSecureStore.cpp
      WindowsCryptoProvider.cpp
      WindowsAppDataPathProvider.cpp
    CMakeLists.txt
  harmony/
    include/
    src/
      HarmonyCryptoProvider.cpp
      HarmonySecureStore.cpp
      HarmonyNetworkBridge.cpp
      napi/
    CMakeLists.txt
  linux/
    include/
    src/
      LinuxHttpClient.cpp
      LinuxSecureStore.cpp
      LinuxCryptoProvider.cpp
      XdgAppDataPathProvider.cpp
    CMakeLists.txt
  mock/
    include/
    src/
    CMakeLists.txt

apps/
  cli/
  harmony/
    AppScope/
    entry/
    hvigorfile.ts
    oh-package.json5
    build-profile.json5

tests/
  unit/
  integration/
  platform/

docs/
  ARCHITECTURE.md
  BUILD.md
  PORTING.md
  SECURITY.md
  DEPENDENCIES.md
应从 core/ 移出的代码
当前代码 建议去向 原因
core/src/Net/WinHttpClient.cpp platform/windows Windows 专用 WinHTTP 实现
core/include/UBAANext/Net/WinHttpClient.hpp platform/windows/include 包含 windows.h/winhttp.h，非 Core 公共 API
CryptoProvider.cpp 中 Windows BCrypt/CryptoAPI 分支 platform/windows 平台加密实现
CryptoProvider.cpp 中 OH Crypto 分支 platform/harmony 依赖 OHOS SDK
VenueReservationService 中读取 WinHttpClient CookieJar 的逻辑 抽象为 ICookieStore/INetworkStack 后留 Core 调用接口 Core 不能 dynamic_cast 具体平台类
CLI PlainFileStore 的 DPAPI 加密逻辑 platform/windows/DpapiSecureStore DPAPI 是平台安全存储，不应混在 CLI 文件存储中
main.cpp 中 get_app_data_dir 平台路径分支 platform/*/AppDataPathProvider CLI 不应知道所有平台路径规则
可以保留在 core/ 的代码
代码 原因
Base/Error/Result/Types 平台无关基础层
Model 数据结构平台无关
Parser 只要不依赖平台 API，可保留
Protocol URL/请求构造逻辑 可保留，但不得访问平台 Cookie/重定向实现
Service 业务编排 可保留，但只依赖接口
IHttpClient/ISecureStore/ICryptoProvider 等接口 Core 应提供抽象边界
MemoryCacheStore 内存实现平台无关，可保留或移入 mock/common
6. 分阶段整改路线
阶段 0：冻结新功能，只补文档和风险清单
项目 内容
目标 停止扩展新业务功能，先把跨平台与安全风险文档化，避免继续加深 Core/Windows 耦合
涉及文件 docs/architecture/overall-architecture.md、docs/architecture/module-boundaries.md、docs/build/linux-build-plan.md、docs/apps/harmony-arkui.md、docs/licensing/third-party-notices.md
预计修改类型 文档更新：架构现状、风险清单、平台支持矩阵、禁止新增平台 API 到 Core 的规则
风险 文档更新不会修复实际问题，但能防止误判当前成熟度
验收标准 文档明确列出 P0/P1/P2；明确 Core 当前并非完全平台无关；明确 Harmony/Linux 当前状态
是否需要构建/测试验证 不需要；只需文档 review
阶段 1：抽象平台接口，移除 Core 对 WinHttpClient 的直接依赖
项目 内容
目标 建立 Interface 层，移除 Core 对 WinHttpClient、CookieJar 具体实现、重定向控制的依赖
涉及文件 core/include/UBAANext/Net/HttpClient.hpp、core/include/UBAANext/UBAANext.hpp、core/src/Service/VenueReservationService.cpp、core/CMakeLists.txt、apps/cli/src/main.cpp
预计修改类型 新增 ICookieStore、IRedirectController、INetworkStack、IPlatformCapabilities；删除 Core 中 include WinHttpClient；调整 VenueReservationService 注入依赖
风险 CGYY/SSO 流程容易回归；Cookie 行为需要测试覆盖
验收标准 core/ 下不再出现 #include <UBAANext/Net/WinHttpClient.hpp>；Core target 不再编译 WinHttpClient.cpp；VenueReservationService 不再 dynamic_cast WinHttpClient
是否需要构建/测试验证 需要；至少 Windows 单元测试、CLI mock 集成测试、CGYY 相关测试；Harmony/Linux 可先配置级验证
阶段 2：补 Linux mock-only preset 和基础构建路径
项目 内容
目标 让 Linux 至少能构建 Core + Mock + mock-only CLI，不承诺真实登录
涉及文件 CMakePresets.json、cmake/UBAANextOptions.cmake、docs/build/linux-build-plan.md、apps/cli/src/main.cpp
预计修改类型 新增 linux-ninja-gcc-debug / linux-ninja-clang-debug preset；增加 UBAANEXT_REAL_NETWORK 开关或平台能力判定；文档声明 Linux mock-only
风险 可能暴露 MSVC 专用函数、路径、警告 -Werror 问题
验收标准 Linux preset 存在；真实网络未实现时 CLI 返回明确 UnsupportedPlatform，不出现 nullptr http 崩溃风险
是否需要构建/测试验证 需要；Linux 配置和 mock-only 构建/测试；但本计划阶段不执行
阶段 3：补 SecureStore / CryptoProvider 平台策略
项目 内容
目标 消除明文 session/token fallback，统一加密能力错误模型
涉及文件 apps/cli/src/PlainFileStore.cpp、core/include/UBAANext/Storage/SecureStore.hpp、core/include/UBAANext/Crypto/CryptoProvider.hpp、core/src/Crypto/CryptoProvider.cpp、apps/cli/src/main.cpp
预计修改类型 WindowsDPAPISecureStore、HarmonySecureStore、LinuxSecureStore；CryptoProvider 分平台 target；未实现平台 fail-fast
风险 登录态迁移、旧 session.dat/cookies.dat 兼容、用户数据丢失风险
验收标准 非 Windows 不再 encrypted=false 保存真实 session；Linux 未实现安全存储时真实登录被禁用；Crypto fallback 不再静默参与真实流程
是否需要构建/测试验证 需要；Windows DPAPI 回归测试、Harmony Native 构建、Linux mock-only 测试；真实平台安全存储需设备/系统验证
阶段 4：补 HarmonyOS ArkUI / NAPI 工程边界
项目 内容
目标 建立 HarmonyOS App 工程，但通过稳定 C API/NAPI 调用 Core，不直接暴露 C++ 内部类
涉及文件 docs/apps/harmony-arkui.md、docs/api/napi-api-plan.md、docs/architecture/binding-design.md、CMakePresets.json、未来 apps/harmony/
预计修改类型 新增 apps/harmony 工程、NAPI bridge、C API facade、Harmony platform adapter；调整 OpenHarmony preset 依赖路径
风险 ArkTS/NAPI 生命周期、线程模型、异步回调、错误映射、敏感数据跨 JS/C++ 边界泄漏
验收标准 apps/harmony 存在 hvigorfile.ts、oh-package.json5、build-profile.json5、module.json5；ArkUI 只依赖 NAPI facade；Core 不依赖 ArkUI/Hvigor
是否需要构建/测试验证 需要；DevEco build、ArkTS 静态检查、NAPI smoke、设备/模拟器启动验证
阶段 5：补安全审计、secret scan、license notices、日志脱敏
项目 内容
目标 发布前补齐安全和合规底线
涉及文件 .gitignore、tools/live-smoke.ps1、apps/cli/src/SecurityRedaction.cpp、docs/licensing/third-party-notices.md、vcpkg.json、NOTICE
预计修改类型 secret scan 流程、日志脱敏统一入口、第三方许可证清单、敏感文件审计说明
风险 真实账号、Cookie、Token 可能出现在本地日志、CI 输出、crash dump 或 issue 附件中
验收标准 third-party notices 包含 nlohmann-json、Catch2、平台 SDK 说明；live smoke 输出脱敏；CLI 错误和 JSON 输出不包含 password/token/cookie/session；.env 不被追踪
是否需要构建/测试验证 部分需要；secret scan、脱敏单元测试、license review；不一定需要完整构建
7. 推荐优先级顺序
先做阶段 0：把风险和边界写清楚，冻结新增平台耦合。
立即做阶段 1：这是跨平台架构能否成立的关键，优先移除 Core -> WinHttpClient。
随后做阶段 2：用 Linux mock-only preset 作为“非 Windows 可移植性探针”。
再做阶段 3：解决真实登录所需的 Crypto/SecureStore 安全底座。
最后推进阶段 4/5：HarmonyOS App 工程与发布级安全/合规。
一句话总结：当前最需要先修的不是 ArkUI 页面，而是 把 WinHTTP、Cookie、重定向、Crypto、SecureStore 从 Core 中剥离成平台 Adapter；否则 HarmonyOS 和 Linux 后续都会被 Windows 具体实现牵制。
