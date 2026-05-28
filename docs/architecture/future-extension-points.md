# 未来扩展点 (Future Extension Points)

本篇文档描述 `UBAANext` 架构中为后续扩展所预留的依赖注入接口、模块化插件结构及自定义平台适配的开发者接入指南。

## 1. 架构的开放性设计（依赖注入）

`UBAANextCore` 在构筑之初就全面摒弃了硬编码外部引擎的做法。核心业务的驱动全部通过构造函数进行**依赖注入（Dependency Injection）**。

因此，开发者在无需修改任何 Core 源码的情况下，仅需在平台壳层（Platform Shell）层继承并实现相关的抽象接口类，即可对整个核心库的网络、加解密和存储特性进行无缝替换。

---

## 2. 核心可扩展接口 (Core Extensibility Interfaces)

以下是四个为未来版本规划的可替换关键扩展点：

### 2.1 自定义网络栈适配器 (Custom IHttpClient & INetworkStack)
*   **目标**：如果宿主环境（如某些嵌入式物联网设备或严格的企业代理环境）不被允许链接 libcurl 静态库，开发者可以实现此扩展点。
*   **接入方式**：
    1.  继承并实现 [HttpClient.hpp](file:///d:/Code/Cpp/UBAANext/core/include/UBAANext/Net/HttpClient.hpp) 中的 `IHttpClient`；
    2.  利用宿主平台原生的网络库（如 Windows WinHTTP、macOS URLSession、Android OkHttp、或 HarmonyOS 原生的 `@ohos.net.http` 接口）重写 `send_request` 异步或同步模型；
    3.  将此自定义的 `IHttpClient` 注入到 `AuthService` 或各业务 `Service` 中。

### 2.2 自定义安全存储引擎 (Custom ISecureStore)
*   **目标**：当前 Windows 已支持 DPAPI，但对于一些特定的安全容器（如加密的 SQLite 数据库、硬件 HSM 安全芯片、或是企业级远程 KMS 服务），可以无缝对接。
*   **接入方式**：
    1.  继承并实现 [SecureStore.hpp](file:///d:/Code/Cpp/UBAANext/core/include/UBAANext/Storage/SecureStore.hpp) 中的 `ISecureStore`；
    2.  实现 `set_string`、`get_string`、`remove` 以及物理刷盘接口 `flush`；
    3.  保证在写入敏感凭据时满足数据加密落盘合规，即可安全注入 `SessionManager` 使用。

### 2.3 自定义加解密计算提供者 (Custom ICryptoProvider)
*   **目标**：当前加解密模块桥接了 OpenSSL。如果未来为了追求极简体积或是国产化合规（如支持国密 SM2/SM3/SM4 算法），可以替换此引擎。
*   **接入方式**：
    1.  重构并继承 `core/include/UBAANext/Crypto/` 下的 `ICryptoProvider`；
    2.  利用系统原生算法（如 Windows CNG、Linux Kernel Crypto API、或第三方国密算法库）实现 AES 加解密、MD5 / SHA-256 哈希、以及 WebVPN 所需的 Base64 编码规范；
    3.  通过 `UBAANext::install_crypto_provider` 注入到 Core 全局上下文中。

---

## 3. 新业务域（Service / Parser）的插件式集成

当北航校园系统上线全新板块（如“博雅选课扩展”、“云盘附件操作”）时，核心库提供了一套高度一致的业务服务与解析器骨架，开发者可以按照以下规范进行扩展接入：

```
+-------------------------------------------------------------+
|                        业务插件集成模板                      |
+-------------------------------------------------------------+
 1. 声明数据模型 : core/include/UBAANext/Model/NewService.hpp
 2. 声明解析策略 : core/include/UBAANext/Parser/NewParser.hpp
 3. 编写业务服务 : core/include/UBAANext/Service/NewService.hpp
 4. C ABI 桥接   : bindings/c/src/UbaaNative.cpp (导出 API)
```

1.  **强类型解析器**：在 `Parser` 层仅编写纯 C++ 的网页 HTML 正则提取或 JSON 反序列化逻辑，不耦合网络 I/O。
2.  **网络与缓存编排**：在 `Service` 层继承 `BaseService`，通过注入的 `IHttpClient` 触发请求，并向 `ICacheStore` 提供 TTL 缓存编排。
3.  **C ABI 接口绑定**：在 `bindings/c` 中补充导出对应的 C 风格符号，用标准的 JSON Envelope 包装服务层返回的强类型模型，提供给外部调用。
