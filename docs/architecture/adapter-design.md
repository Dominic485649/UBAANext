# 适配器设计 (Adapter Design)

本篇文档详述 `UBAANext` 原生核心库在处理操作系统级平台能力差时所采用的**桥接与适配器模式 (Bridge & Adapter Patterns)**，以及数据存储和网络栈的适配细节。

## 1. 为什么需要适配器设计

`UBAANextCore` 的核心业务逻辑（如课表解析、SSO 认证流程）被设计为极致的纯 C++ 跨平台实现，它不能直接知道任何特定操作系统的 SDK API（如 Windows `CryptProtectData` 或 鸿蒙 `SecKey` 等）。

为了使用各操作系统底层的安全凭证箱、持久化网络连接和本地文件路径能力，我们采用了经典的 **接口与实现解耦模式（Bridge/Adapter）**：

```
+----------------------------------------+
|             UBAANextCore               |
|  (只依赖抽象接口: ISecureStore, ... )   |
+----------------------------------------+
                   |
                   v (依赖注入)
+----------------------------------------+
|           Platform Adapter             |
|   (具体平台实现: DpapiSecureStore, ... ) |
+----------------------------------------+
```

---

## 2. 存储适配器 (Storage Adapters)

持久化存储以 [SecureStore.hpp](file:///d:/Code/Cpp/UBAANext/core/include/UBAANext/Storage/SecureStore.hpp) 中的 `ISecureStore` 为抽象边界。根据运行时的操作系统环境，系统会动态装配不同的具体存储适配器：

### 2.1 Windows 平台：`DpapiSecureStore`
*   **物理机制**：桥接 Windows 本地 `CryptProtectData` 与 `CryptUnprotectData`（DPAPI 加密 API）。
*   **安全防线**：将会话明文序列化为内存串后，直接调用 DPAPI，由 Windows 内核基于当前登录用户凭据生成硬件/系统级的不可读密文，最后写入本机的临时会话文件。这确保了即便会话文件被他人拷走，在非同一用户或非同一台电脑上也绝对无法解密，彻底防止凭据失窃。

### 2.2 Linux 平台：`SecretServiceSecureStore`
*   **物理机制**：桥接 Linux 桌面的 Libsecret（桥接系统 D-Bus 与 Secret Service API）。
*   **安全防线**：使用 Gnome Keyring 或 KWallet 在内核层面提供硬件受保护的私钥存储，阻断在普通纯文本文档中存盘的风险。

### 2.3 鸿蒙平台 / 兜底平台：`VolatileSecureStore` 与 `UnsupportedSecureStore`
*   **开发预研**：当前鸿蒙工程底座 capability 未连接 OSKeychain 时，核心库在 `platform/harmony/src` 下提供了 `UnsupportedSecureStore`：它实现 `ISecureStore` 接口，但会在调用 `set_string` 或 `clear` 时直接**抛出异常**（Fail-Closed 原则，拒绝隐式向明文落盘降级）。
*   **C ABI 缓冲适配**：在 C ABI 桥接层，为了规避直接抛出异常崩溃的问题，`bindings/c` 自定义了在内存中运行的 `VolatileSecureStore`：它将键值对保存在 Context 内存桶的 `std::unordered_map` 中。在 Context 存活期内能够正常处理 Cookie，一旦进程销毁，数据全部挥发，既提供了基础运行保障，又阻断了物理介质上的泄露。

---

## 3. 网络与 Cookie 适配器 (Network Adapters)

网络传输以 `INetworkStack`、`IHttpClient` 和 `ICookieStore` 为边界。底层网络能力全部通过 libcurl 进行适配包装：

### 3.1 `CurlNetworkStack` 适配器
*   **物理机制**：包装了开源的 `libcurl` 引擎（见 `platform/common/curl/`）。
*   **Cookie 注入与提取**：libcurl 默认使用进程内存维护 Session Cookie。`CurlNetworkStack` 实现了 `ICookieStore` 接口，在请求前自动从底层 `ISecureStore` 中序列化读取以前保存的网关 Cookie，注入到 curl 句柄中；并在网络请求返回后，拦截 `Set-Cookie` 头部，将最新的 Cookie 存回安全存储区。
*   **重定向精细控制 (Redirect Control)**：原生的 libcurl 会自动跟随 `302` 跳转。但对于北航 SSO 复杂的 `jumpMyCenter` 阶段性多步重定向，`CurlNetworkStack` 实现了 `IRedirectController`，通过重写 `CURLOPT_FOLLOWLOCATION` 逻辑，允许 `AuthService` 手动接收每次 302 重定向后的 Header 并解析 `loginName` 关键参数。

---

## 4. 缓存适配器 (Cache Adapters)

以 `ICacheStore` 为抽象，主要提供无过期时间或带 TTL（存活时间，如 10 分钟）的纯数据缓存适配：

*   **当前实现：`MemoryCacheStore`**：
    *   在内存中通过线程安全的 `std::unordered_map` 并结合 `std::chrono` 计时器实现内存缓存。
    *   主要用于暂时存放今日课表、考试表等频繁请求的静态页面数据。
    *   **安全红线**：根据缓存策略，`ICacheStore` **绝不允许被用来存放用户明文密码、登录 CAPTCHA、Authorization 票据或 Session ID**，缓存只服务于无权判定（Non-Authoritative）的静态业务查询，数据过期即自动物理擦除。
