# 决策 0001: 使用 C++17/C++20 作为基线

## 状态

已接受

## 背景

UBAA Next 需要同时覆盖 Windows/MSVC 与 OpenHarmony Native clang++，因此标准基线需要：
- 在 MSVC 与 Clang 上稳定可用
- 受 CMake 和当前 SDK 工具链支持
- 避免依赖 OpenHarmony Native 可能尚不完整的 C++23 标准库组件
- 保留后续切换到 C++20 的空间

## 决策

默认使用 **C++17** 作为项目基线，并通过 `UBAANEXT_CXX_STANDARD=20` 支持 C++20 构建。

## 理由

1. C++17 是 MSVC 与 OpenHarmony Native clang++ 都可稳定支持的最低现代基线。
2. 项目自带 `Result<T>`，不再依赖 C++23 `std::expected`。
3. CLI 已移除 `std::format`、`std::print` 等 C++20/23 标准库依赖。
4. C++20 可作为可选构建模式验证未来迁移，但不能成为默认要求。

## 影响

- `CMAKE_CXX_STANDARD` 由 `UBAANEXT_CXX_STANDARD` 控制，允许值为 `17` 或 `20`。
- `CMAKE_CXX_STANDARD_REQUIRED` 设置为 `ON`。
- `CMAKE_CXX_EXTENSIONS` 设置为 `OFF`。
- Windows 专用 `WinHttpClient` 仅在 `WIN32` 下编译；非 Windows/OpenHarmony 可编译核心、Mock 与 CLI 离线模式。
