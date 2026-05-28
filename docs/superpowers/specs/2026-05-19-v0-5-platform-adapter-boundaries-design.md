# UBAA Next v0.5 阶段 1-5 平台适配边界设计

> 当前仓库版本阶段为 `v0.3.0`。本文件是 v0.5 平台适配方向的历史设计草案，不代表 v0.3 当前稳定承诺；执行前应重新对照 `docs/02-roadmap.md` 和当前代码状态。

## 背景

v0.5 阶段 1-5 的目标是先完成 Core 与平台能力的硬边界，而不是继续推进真实校园协议。当前 Core 仍包含 WinHTTP、平台 Crypto、SecureStore、Cookie/Redirect、用户文件读取和平台链接项。继续在这些边界未清理前扩展真实协议，会扩大 Windows-only 依赖、HarmonyOS/Linux 迁移成本，以及凭据、Cookie、session、token 泄漏风险。

本设计覆盖阶段 1-5：CMake target 边界、Curl Network/Cookie/Redirect、OpenSSL Crypto、SecureStore、安全上传边界。实施采用大爆炸迁移：一次性重建目录、target 和关键接口，阶段 1-5 全部完成后统一验收。中间状态不承诺每个子阶段都可独立构建或测试，但每个主题改动必须形成可审查、可回滚的 Git checkpoint commit。

## 非目标

- 不恢复新的真实协议能力。
- 不运行 live smoke。
- 不读取 `.env` 或真实凭据。
- 不把 WinHTTP、OH Crypto、CryptoAPI/BCrypt、明文 Cookie 文件作为 Core 或长期 fallback 路径保留。
- 不把 Linux 定义为永久 mock-only 平台。
- 不让 OpenSSL 代替 SecureStore。

## 迁移策略与验收边界

阶段 1-5 作为一个重构窗口统一落地。最终验收目标是：`UBAANextCore` 不再编译或链接平台实现，不 include WinHTTP、`curl/curl.h`、`openssl/*`、BCrypt/CryptoAPI、OH Crypto/HUKS/libsecret 等平台头；真实网络的长期目标是统一经 `platform/common/curl`，真实算法统一经 `platform/common/openssl`，安全存储由各 OS adapter 提供。当前阶段允许 WinHTTP 仅作为 `platform/windows` 内部过渡 adapter 存在，不允许进入 Core 或作为跨平台 fallback。

大爆炸迁移不等于单个不可审查提交。阶段 1-5 中途不要求每个子阶段独立构建或测试通过，但每个主题改动必须形成 checkpoint commit，确保审查和回滚边界清晰。checkpoint 至少包括：

1. CMake / platform skeleton；
2. Curl network / Cookie / Redirect；
3. OpenSSL crypto；
4. SecureStore / capability；
5. Upload boundary；
6. final redaction / tests。

每个 checkpoint 后执行 `git status`、`git diff --stat`、`git diff --check`，并确认 diff 中没有 `.env`、真实凭据、Cookie、token、session、ticket、password 或验证码。

未完成平台能力必须 fail-fast，返回 capability error，不能静默 fallback，不能明文保存真实凭据、Cookie、session、ticket 或 token。

## 目标架构

### Core

Core 保留平台无关能力：Result/Error、模型、Parser、Protocol、Service 编排、纯工具和接口定义。`UBAANextCore` 只依赖平台无关库，例如 JSON。Core 不链接 OS SDK、libcurl、OpenSSL、WinHTTP、DPAPI、HUKS 或 libsecret。

Service、Protocol、Parser 只能依赖接口，例如 `IHttpClient`、`INetworkStack`、`ICookieStore`、`IRedirectController`、`ICryptoProvider`、`ISecureStore`、`IAppDataPathProvider`、`IPlatformCapabilities`。它们不能依赖任何具体 platform provider。

### Platform common

`platform/common/curl` 提供统一网络实现：`CurlRuntime`、`CurlHttpClient`、`CurlCookieStore`、`CurlRedirectController`、`CurlNetworkStack`、`CurlErrorMapper`。Windows、Linux、HarmonyOS 的真实网络都组装这套 adapter。

`platform/common/openssl` 提供统一算法实现：`OpenSslCryptoProvider`、`OpenSslRuntime`、`OpenSslErrorMapper`。Windows、Linux、HarmonyOS 的真实 `ICryptoProvider` 都使用该 provider。

### OS platform

`platform/windows`、`platform/linux`、`platform/harmony` 负责 OS 能力组装：SecureStore、AppDataPath、capabilities，以及 Curl/OpenSSL provider 注入。

- Windows SecureStore 使用 DPAPI。
- Linux SecureStore 使用 libsecret / Secret Service。
- HarmonyOS SecureStore 使用 HUKS。
- HarmonyOS 不使用 OH Crypto / CryptoArchitectureKit 作为目标 crypto provider。

### App 与 binding

`apps/cli`、未来 `apps/harmony`、`bindings/c`、`bindings/napi` 负责用户输入、文件读取、UI/NAPI 生命周期、错误脱敏和 platform factory 选择。文件路径、URI、sandbox、媒体库权限和本地 I/O 不进入 Core。

## Network、Cookie、Redirect

Core 中业务请求只通过平台无关接口表达，不直接接触 libcurl。`CurlRuntime`、`CurlHttpClient`、`CurlCookieStore`、`CurlRedirectController`、`CurlErrorMapper` 是唯一允许持有 `CURL*`、`CURLSH*`、`curl_mime*`、`CURLcode`、libcurl cookie 原始行和底层 TLS error 的层，且全部位于 `platform/common/curl`。Core、Service、Protocol、Parser、CLI JSON、NAPI 输出和测试快照都不能暴露这些类型或原始内容。

Cookie 分两层处理。libcurl cookie engine 只负责请求期内的 Cookie 解析、匹配和发送。真实 Cookie 持久化必须走 `ICookieStore + ISecureStore`，由平台安全存储保护。`CurlCookieStore` 可以从 libcurl cookie engine 导入/导出 Cookie，但不能把 libcurl `COOKIEJAR` 明文文件作为唯一或长期持久化机制。

Redirect 必须支持 per-request 控制，替代原先 `RedirectGuard` / `scoped_redirects` 行为。`IRedirectController` 或 `HttpRequest` options 至少表达：

- `follow_redirects`；
- `max_redirects`；
- 是否允许读取 30x `Location`；
- 是否允许 POST 在 301/302/303 后切换为 GET；
- 是否限制 redirect 协议为 HTTP/HTTPS。

Windows、Linux、HarmonyOS 的长期目标都走 `CurlNetworkStack`。差异只体现在平台 factory 传入的 TLS、CA、proxy、app-data、SecureStore 配置。当前 Windows 仍保留 `platform/windows` 内部 WinHTTP adapter 作为过渡实现；它不得被 Core include、编译或链接，也不得作为非 Windows fallback。后续完成 CurlNetworkStack gate 后再移除 WinHTTP。

## Crypto 与 SecureStore

Crypto 和 SecureStore 是不同能力。`ICryptoProvider` 负责协议算法，例如 MD5、SHA-1、AES、RSA 和编码辅助。`ISecureStore` 负责凭据、session、token、ticket、Cookie 等敏感数据的系统级安全保存。

Core 保留 `ICryptoProvider` 接口和纯编码工具，移除 Windows BCrypt/CryptoAPI、OH Crypto、fallback crypto 参与真实协议的路径。真实算法统一使用 `platform/common/openssl` 的 `OpenSslCryptoProvider`。OpenSSL 不可用时真实 crypto / 真实登录 fail-fast，不回退到 BCrypt、CryptoAPI、OH Crypto 或自写 fallback。

SecureStore 由 OS adapter 实现。未完成真实安全存储的平台禁止真实登录、禁止持久化真实 session/token/Cookie，也不能用明文文件伪装成 encrypted store。Mock/unsupported provider 只允许用于离线测试、mock 模式或明确 unsupported 路径，不能参与真实协议。

OpenSSL、DPAPI、HUKS、libsecret 的底层错误都必须在 platform 层映射为 UBAANext `ErrorCode` 和脱敏 message。Core public API、CLI JSON、NAPI 输出、测试快照不暴露底层错误码、密钥材料、密文 blob、provider 内部结构或敏感明文。

## Upload 与 app shell 边界

Core 不再读取用户本地文件，也不理解平台文件权限模型。现有 Core service 中直接 `std::ifstream` 读取图片、从本地路径推断 filename/MIME 的职责移出 Core。

新的上传边界是：

- Core 接收平台无关的 `UploadPart`：`field_name`、`filename`、`content_type`、`bytes`。
- Service 只表达业务协议需要哪些 multipart 字段。
- CLI 负责解析用户输入路径、读取文件 bytes、推断 MIME/filename，然后把 `UploadPart` 传给 Core。
- HarmonyOS ArkUI / NAPI 负责文件选择、授权、URI/media library/sandbox 读取，再把 bytes 传给 Core 或 C/NAPI facade。
- Curl adapter 只负责发送已经准备好的 bytes，不负责选择或读取用户文件。

错误输出和日志不能泄露本地完整路径、用户目录、文件内容、图片 EXIF、上传 token 或表单敏感字段。Core 可以校验 `UploadPart` 是否为空、大小是否超限、必需字段是否存在，但不做任何平台文件系统访问。

## CMake target 与依赖可见性

目标 CMake 依赖方向是 app/binding → platform → core，不能反向。

- `UBAANextCore` 只编译 Core 平台无关代码。
- `UBAANextPlatformCurl` 编译 `platform/common/curl`，PRIVATE 链接 libcurl。
- `UBAANextPlatformOpenSSL` 编译 `platform/common/openssl`，PRIVATE 链接 OpenSSL。
- `UBAANextPlatformWindows`、`UBAANextPlatformLinux`、`UBAANextPlatformHarmony` 编译 OS adapter，PRIVATE 链接平台库，并组合 Curl/OpenSSL/common provider。
- `UBAANextPlatformMock` 提供 mock/unsupported provider，用于离线测试和 unsupported capability。
- `ubaa` CLI 链接 Core + 对应 platform target + mocks，并负责组装 `ServiceFactory`。

平台库不能通过 PUBLIC/INTERFACE 依赖泄漏给 `UBAANextCore`。Core target 中不得再出现 `WinHttpClient.cpp`、`winhttp`、`crypt32`、`bcrypt`、`libohcrypto.so`、`OpenSSL::Crypto`、`OpenSSL::SSL` 或 `curl`。

## 错误、能力与 fail-fast

所有真实能力都通过 capability 显式声明。`IPlatformCapabilities` 描述当前平台是否支持真实网络、Cookie 安全持久化、redirect 控制、OpenSSL crypto、SecureStore、AppDataPath、upload bytes 和 live login。

Service/Protocol 在进入真实登录、真实网络、真实写操作前检查 capability。缺失时返回 `UnsupportedPlatform`、`UnsupportedNetwork`、`UnsupportedCrypto`、`UnsupportedSecureStore` 等可诊断错误。Linux/HarmonyOS 如果尚未通过 Curl/OpenSSL/SecureStore gate，真实登录和真实持久化必须 fail-fast。

写操作仍保持后置，不在阶段 1-5 恢复。如果阶段 1-5 改到写操作相关 service，只能保留离线/mocked 行为，不新增真实副作用。

## 脱敏边界

CLI、NAPI、test snapshot 和 debug 日志都不得输出 Cookie、Set-Cookie、Authorization、token、ticket、session、password、验证码，以及带敏感 query 的完整 URL。debug 日志也必须经过 redaction，而不是只限制 release 输出。

URL、Header、Query 参数和 Cookie 的脱敏边界至少覆盖：

- request/response 日志；
- platform adapter error message；
- CLI JSON；
- NAPI error object；
- test snapshot；
- live smoke 输出；
- debug callback。

## 测试与发布门

因为采用大爆炸迁移，构建与测试验收在阶段 1-5 全部完成后统一执行。checkpoint commit 只承担审查、回滚和 diff hygiene 责任，不要求每个 checkpoint 独立构建或测试通过。

### Checkpoint hygiene

每个 checkpoint commit 前后都必须确认：

- `git status` 能清楚显示本 checkpoint 的文件范围；
- `git diff --stat` 与 checkpoint 主题匹配；
- `git diff --check` 无 whitespace/error；
- diff 不包含 `.env`、真实账号、密码、Cookie、Set-Cookie、Authorization、token、ticket、session、验证码或真实敏感 URL；
- checkpoint message 能说明主题边界和回滚含义。

### 最终统一验收

阶段 1-5 完成、准备合并前统一执行：静态边界检查、CMake target 检查、offline build/tests、redaction tests、capability fail-fast tests。

### 静态边界检查

- 搜索 `core/`：不得出现 `windows.h`、`winhttp.h`、`bcrypt.h`、`wincrypt.h`、`CryptoArchitectureKit`、`curl/curl.h`、`openssl/`、`libsecret`、`OH_Huks`、`OH_Crypto` 等平台头。
- 检查 `core/CMakeLists.txt`：不得链接 `winhttp`、`crypt32`、`bcrypt`、`libohcrypto.so`、`curl`、`OpenSSL::Crypto`、`OpenSSL::SSL`。
- 检查 Service/Protocol/Parser：不得 `dynamic_cast` 到平台具体类，不得读取本地用户文件。

### 离线构建与单元测试

优先运行与 Core、mock platform、CLI service factory、Network/Cookie/Redirect/Crypto/SecureStore/Upload 边界相关的局部测试。默认不读取 `.env`，不访问校园系统。测试快照必须经过 redaction，不包含 Cookie、token、ticket、session、password、验证码或敏感 URL query。

### live smoke 发布门

阶段 1-5 不主动恢复真实协议 live smoke。后续 L1 live 只读必须显式环境开关；L2/L3 写操作必须人工确认。CI 不允许配置真实凭据，不运行 live smoke。

## 成功标准

- `UBAANextCore` 不编译或链接平台实现。
- Core public headers 不 include 平台头、`curl/curl.h` 或 `openssl/*`。
- WinHTTP 不进入 Core；当前仅允许作为 `platform/windows` 内部过渡 adapter，后续 CurlNetworkStack gate 完成后移除。
- Cookie 长期持久化只通过 `ICookieStore + ISecureStore`。
- Redirect 支持 per-request 控制。
- 真实 crypto 统一通过 `OpenSslCryptoProvider`。
- SecureStore 按 OS adapter 注入，未实现时 fail-fast。
- Core 不读取用户本地文件。
- CLI/NAPI/test/debug 输出通过 redaction。
- 默认测试离线，不读取 `.env`，不访问真实校园系统。
- 阶段 1-5 中途按主题形成 checkpoint commit，最终合并前统一执行完整验收。
