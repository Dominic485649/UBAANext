# 敏感数据处理策略 (Sensitive Data Policy)

本篇文档详述 `UBAANext` 原生核心库在处理用户隐私和敏感教学业务数据时的安全防护机制与全局脱敏策略。

## 1. 脱敏设计目标与共享引擎

在与校园系统交互、执行命令行 CLI 输出、捕获网络异常及生成本地诊断调试日志（Diagnostics）时，程序可能会接触并记录一些核心敏感信息。

为防止用户私隐在非预期的边界泄露，核心库在 [SecurityRedaction.hpp](file:///d:/Code/Cpp/UBAANext/core/include/UBAANext/Security/SecurityRedaction.hpp) 和 [SecurityRedaction.cpp](file:///d:/Code/Cpp/UBAANext/core/src/Security/SecurityRedaction.cpp) 中实现了一套高效、无状态的共享脱敏引擎（`UBAANext::Security`）。

任何要通过 C ABI 输出、控制台打印、或者向下游抛出 `std::exception::what()` 的字符串，必须强制经过此引擎进行敏感字符的扫描与一律替换为 `[REDACTED]`。

---

## 2. 脱敏过滤标准规则

脱敏引擎运用字符状态机与精确字符串扫描技术，对输入文本执行以下 **五类全局过滤规制**：

### 2.1 敏感键值对（Key-Value Fields）的拦截
当文本中出现以下键（不区分大小写，且前后需满足分隔界限符以防误伤其他单词），其后紧跟的 `=` 或 `:` 之后的内容将被替换为 `[REDACTED]`，直到遇到终止符（如 `&`, 空格, 换行, `}`, `"`, `'` 等）：

*   **身份凭据类**：`username`, `account`, `student_id`, `studentId`, `password`, `passwd`, `pwd`
*   **会话票据类**：`token`, `ticket`, `cas`, `execution`, `session`, `session_id`, `captcha`, `验证码`
*   **网络与凭证头**：`cookie`, `authorization`, `cgauthorization`
*   **文件与路径类**：`photo_path`, `path`, `filename`, `file`
*   **教学与场馆专用敏感参数**：`lock_code`, `lockCode`, `lockcode`（锁码）、`booking_id`, `bookingId`（预订ID）、`place`, `location`（场馆地点）

### 2.2 HTTP 协议头部的精准抹除
引擎对标准的 HTTP 头域输出进行直接整行覆盖抹除，匹配以下前缀的头部后，冒号 `:` 之后的整行参数值均会被替换为 `[REDACTED]`：
*   `Cookie:`
*   `Set-Cookie:`
*   `Authorization:`
*   `cgAuthorization:`

### 2.3 URL 查询参数与代理凭证屏蔽
*   **代理 URL 脱敏**：如果输入的代理服务器配置中包含用户凭证（如 `socks5://user:pass@host:port`），`redact_proxy_url` 会将其自动处理为 `socks5://[REDACTED]@host:port`，保留主网络节点同时隐去凭证。
*   **URL Query 脱敏**：扫描文本中出现的任何 `http://` 或 `https://` 的网络请求 URL。一旦发现带有 `?` 号，立即将 `?` 之后到 URL 终止（或遭遇 `#` 锚点）前的全部查询参数强行擦除为 `[REDACTED]`，从而阻断 SSO 在重定向跳转时携带的敏感 Ticket 和 Token 泄露在日志中。

### 2.4 本地文件绝对路径屏蔽
为了防止泄露宿主系统的用户名和内部文件层级架构，引擎对本地绝对路径进行屏蔽：
*   **Windows 路径**：检测任何形如 `盘符:\` 或 `盘符:/`（如 `C:/` 或 `d:\`）开头的 Windows 本地绝对路径，自动整段替换为 `[REDACTED]`。
*   **Linux/Android/鸿蒙路径**：检测以 `/data/`、`/storage/`、`/sdcard/` 开头的文件路径，自动整段擦除为 `[REDACTED]`。

### 2.5 远端大文本与页面泄露（HTML / Form）
当下游系统返回服务异常或出错时，经常会携带大段的 raw HTML 错页或表单数据（其中可能包含明文敏感字段或 CSRF 令牌）。
*   **规则**：引擎一旦在文本中检索到 `<!doctype html`、`<html` 或 `<form` 标记（不区分大小写），将**判定此段文本为 HTML 片段，并将标记位置直至文本结束的全部内容直接截断并替换为 `[REDACTED]`**。

---

## 3. 开发规范与要求

在开发 `UBAANext` 核心服务或新增平台适配层时，开发团队必须严格遵守以下敏感数据处理纪律：

1.  **禁止主动保存明文凭据**：除了调用底层的安全凭证箱（如 Windows DPAPI 或未来鸿蒙原生系统 Secure Store）外，任何本地的配置读写、JSON 缓存都不允许保留用户的原始明文密码。
2.  **异常信息的防御式构建**：编写 `throw std::runtime_error(...)` 抛出异常时，**严禁将原始请求体、解析的 HTML 网页、敏感的 URL query 或是服务器响应直接作为 exception.what() 的内容**。如果有调试需求，必须将其先调用 `redact_sensitive_text` 处理后再行抛出。
3.  **日志脱敏先行**：CLI 侧的诊断流输出和 live-smoke 的测试 runner，必须默认在管道末端调用脱敏引擎，确保保存的自动化测试 log 中不包含任何学生私人隐私数据。
