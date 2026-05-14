# 模块边界

## 模块

| 模块 | CMake Target | 角色 |
|------|-------------|------|
| Core | `UBAANextCore` | 业务逻辑、模型、抽象 |
| Mocks | `UBAANextMocks` | 用于测试的 Mock 实现 |
| CLI | `ubaa` | Windows 命令行入口 |
| Tests | `UBAANextTests` | 单元测试 |

## 依赖规则

```
ubaa (CLI) ──→ UBAANextCore
           ──→ UBAANextMocks

UBAANextTests ──→ UBAANextCore
              ──→ UBAANextMocks
              ──→ Catch2

UBAANextMocks ──→ UBAANextCore

UBAANextCore ──→ （无外部依赖）
```

## 头文件可见性

- **公共头文件**：`core/include/UBAANext/` — 供所有下游 target 使用
- **Mock 头文件**：`mocks/include/UBAANextMocks/` — 供 CLI 和测试使用
- **内部头文件**：`core/src/` — 不对外暴露给下游 target
