# 错误模型

## 错误码

| 错误码 | 描述 |
|--------|------|
| `None` | 无错误 |
| `Unknown` | 未知错误 |
| `InvalidArgument` | 输入参数无效 |
| `NetworkError` | 网络请求失败 |
| `AuthFailed` | 认证失败 |
| `SessionExpired` | 会话已过期 |
| `ParseError` | 响应解析失败 |
| `NotImplemented` | 功能尚未实现 |

## 错误结构体

```cpp
struct Error {
    ErrorCode code;
    std::string message;
};
```

## 用法

```cpp
// 创建错误
auto err = Error(ErrorCode::NetworkError, "Connection timeout");

// 检查错误
if (err) {
    // err.code != ErrorCode::None
}
```

## Result 模式

所有可能失败的操作返回 `Result<T>` 或 `Result<void>`。参见 [Result 类型](result-type.md)。
