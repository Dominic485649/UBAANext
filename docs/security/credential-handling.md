# 凭据处理策略 (Credential Handling Policy)

本篇文档详述 `UBAANext` 原生核心库在处理用户密码、Cookie 令牌、CAPTCHA 验证码及临时会话参数等高风险凭据时的安全性与销毁策略。

## 1. 内存中的凭据生存期与安全限制

任何形式的用户登录凭据在内存中的停留时间应当遵循“**最小可用生命周期**”原则，严禁以全局变量或长寿命静态对象驻留：

1.  **临时凭据销毁**：用户输入的明文密码 `password` 与 `captcha` 均以 `const std::string &` 形式以局部栈参数传入 `AuthService::login_real`。在网络表单打包发送（`POST`）后，随着函数调用栈返回，相关内存会自动析构。
2.  **网络栈脱敏**：在构造 POST 表单时产生的 payload 数据，在底层 `curl` 响应接收完毕后，会立即覆盖擦除临时缓冲区。
3.  **禁止在内存保留明文密码**：`Session` 数据结构和 `SessionManager` 中**只持久化缓存账户信息（学号 `student_id`、显示名 `display_name`）与 Cookie 令牌，绝对不保留用户的明文密码**。任何模块（包括 UI 侧）不得尝试将密码留存在后台。

---

## 2. 基于 ISecureStore 的加密持久化

### 2.1 强力防明文持久化（Fail-Closed）
当登录会话成功建立后，系统将通过 `SessionManager::save_session` 进行序列化存储。
*   在 Windows 操作系统下，系统将自动使用 `DpapiSecureStore`（调用底层 Windows DPAPI 凭据保险箱），将生成的令牌加密落盘。
*   在 Linux 下，如若配置开启，则会桥接至 `SecretServiceSecureStore`；若未配置则会回退为内存级的 `VolatileSecureStore`，**绝对不允许在磁盘明文保存任何会话文件**。
*   如果平台适配器声明了不支持安全存储（例如当前未连接 keychain 的 OpenHarmony 底座返回 `secure_store = false`），底层在尝试进行磁盘持久化时会抛出 `UnsupportedSecureStore` 错误直接失败（Fail-Closed），绝对不会隐式降级回退成明文不设防磁盘存储。

### 2.2 跨会话 Cookie 隔离
*   由 `ICookieStore` 负责存储跨 session 访问所需的 WebVPN 或学校教务系统的会话 Cookie。
*   Cookie 的落盘路径同样受平台 Capability 的安全级别控制（如 `secure_cookie_persistence` 标志）。如果该标志为 `false`，则 Cookie 的生命周期仅约束在当前进程存活期间，退出即销毁。

---

## 3. 会话的主动擦除与登出规范

### 3.1 本地凭据的一键清除
当用户执行登出操作时，`ubaanext_auth_logout`（或核心库中的 `AuthService::logout`）会执行以下彻底清理动作：
1.  **清除临时会话**：清空 `AuthService` 内的 `m_session` 内存镜像；
2.  **物理抹除 SecureStore**：调用 `SessionManager::clear_session`，该函数会遍历并擦除底层的安全凭据箱记录；
3.  **清空网络 Cookie**：强制清空底层 `CurlNetworkStack` 所绑定的 Cookie 罐，并调用 `cookie_store().clear()` 清除磁盘 Cookie 记录。

### 3.2 冷启动失效安全性
由于当前 HarmonyOS C ABI 上层使用的是 `VolatileSecureStore`：
*   所有会话令牌全部寄存在内存的 Context 隔离运行时桶内；
*   当 App 进程退出或被冷关闭时，系统会隐式销毁 Context，**使得会话 Cookie 与账户数据从物理内存中彻底挥发，不留任何持久化磁盘痕迹**，这确保了冷启动状态下用户会话的最大防盗安全性。
