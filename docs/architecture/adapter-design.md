# 适配器设计

> TODO: 本文档将描述平台相关实现的适配器模式。

适配器为特定平台实现 Core 抽象（IHttpClient、ISecureStore、ICacheStore）：

- **v0.1**: MockHttpClient、MockSecureStore
- **v0.2+**: 真实 HTTP 客户端（libcurl/WinHTTP）、DPAPI SecureStore、SQLite CacheStore
