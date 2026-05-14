# Cookie 会话策略

## Cookie 隔离

`CookieJar` 按 host、path、name 存储 Cookie。生成请求头时，只发送与目标 host 匹配的 Cookie：

- host 完全一致时发送。
- 子域请求只匹配父域 Cookie，例如 `jwapp.buaa.edu.cn` 可匹配 `buaa.edu.cn`。
- host 为空的 Cookie 仅在无 host 请求头场景使用，不会自动发送给所有外部域名。

## Cookie 输入校验

为了降低响应头注入和过宽 Cookie 域风险，`CookieJar` 会拒绝：

- 名称、值或路径中包含控制字符的 Cookie。
- 没有点号的域名，例如 `com`。
- 以点开头、以点结尾或包含连续点的域名。
- 包含非字母数字、连字符、点号之外字符的域名。

## 持久化

Windows 下 Cookie 文件使用 DPAPI 加密保存。加载失败时视为无可用 Cookie，不会继续使用损坏的会话数据。

## 重定向

真实登录流程只跟随 HTTPS 且 host 属于 `buaa.edu.cn` 或其子域的重定向。遇到外部域名、明文 HTTP 或异常 host 时返回网络错误。
