# 安全存储

UBAA Next 通过 `ISecureStore` 抽象会话与账号相关数据的持久化方式。

## CLI 存储策略

Windows CLI 使用 `PlainFileStore` 的加密模式保存会话文件：

- 文件位置：`%LOCALAPPDATA%/UBAANext/session.dat`
- 加密方式：Windows DPAPI `CryptProtectData`
- 解密方式：Windows DPAPI `CryptUnprotectData`
- 文件内容：序列化后的 `session.*` 键值对

非 Windows CLI 当前仅用于 OpenHarmony/离线构建验证，不承诺真实凭据持久化安全性；后续平台 Shell 应接入平台安全存储。

## Cookie 存储策略

Windows `WinHttpClient` 使用 DPAPI 加密 `cookies.dat`。Cookie 文件损坏或无法解密时不会回退执行危险操作，只会导致会话丢失并要求重新登录。

## 明文模式限制

`PlainFileStore(encrypted=false)` 仅用于测试或离线开发。真实登录路径在 Windows 下必须启用加密。

## 不存储的内容

- 不保存用户输入的明文密码。
- 不把密码写入配置文件、日志或 JSON 输出。
- `config show` 只输出配置路径、连接模式、代理和缓存开关。
