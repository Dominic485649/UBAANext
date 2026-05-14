# 依赖规则

## 规则

1. **Core 不得依赖平台 Shell** — CLI、ArkUI、Slint 依赖 Core，反之不行
2. **Core 不得依赖 Mock 实现** — Mocks 依赖 Core
3. **公共头文件不得包含 `namespace um = UBAANext;`** — 别名仅在 .cpp/测试/CLI 中使用
4. **不使用全局 `include_directories` 或 `link_directories`** — 使用基于 Target 的 CMake
5. **Core 中不得包含 UI 框架依赖** — 不使用 ArkUI、Slint、Windows API
6. **v0.1 中不使用真实外部依赖** — 仅标准库 + Catch2（用于测试）

## 执行方式

- 基于 CMake Target 的链接强制执行模块边界
- CI 将验证公共头文件中不包含被禁止的 include
