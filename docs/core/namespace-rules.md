# 命名空间规则

## 公共 API

所有公共 API 必须使用:

```cpp
namespace UBAANext {
}
```

## 内部别名

允许在 `.cpp` 文件、测试文件和 CLI 源码中使用:

```cpp
namespace um = UBAANext;
```

## 禁止用法

禁止在 `core/include/UBAANext/` 下的公共头文件中使用 `namespace um = UBAANext;`。

**原因**: `um` 过于简短，可能会污染外部项目的命名空间。

## CMake 目标名称

| 目标 | 用途 |
|------|------|
| `UBAANextCore` | 核心静态库 |
| `UBAANextMocks` | Mock 实现 |
| `ubaa` | CLI 可执行文件 |
| `UBAANextTests` | 测试可执行文件 |

## 文件路径约定

公共头文件: `core/include/UBAANext/<Module>/<File>.hpp`
