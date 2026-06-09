# 第三方声明

本文档记录 UBAA Next 当前直接声明的第三方依赖。完整版本以仓库根目录 `vcpkg.json`、实际 CMake 配置和分发包内容为准。

## 默认依赖

| 依赖 | 用途 | 许可证 |
| --- | --- | --- |
| nlohmann-json | JSON 解析与序列化 | MIT License |
| Catch2 | 单元测试与集成测试框架 | BSL-1.0 |
| curl / libcurl | 真实 HTTP 网络栈 | curl license |
| OpenSSL | 加密 provider | Apache License 2.0 |

## 可选依赖

| 依赖 | 触发条件 | 用途 | 许可证处理 |
| --- | --- | --- | --- |
| Slint | `UBAANEXT_BUILD_DESKTOP=ON`，通过本机包或 `UBAANEXT_FETCH_DEPS=ON` 获取 | 桌面 GUI | 分发前按实际版本和授权模式更新 NOTICE 与 UI 署名 |
| WinFsp | Windows + `UBAANEXT_ENABLE_WINFSP=ON` 且找到 SDK/runtime | Windows mount frontend 预埋 | 当前未形成可分发 adapter；完成后补充许可证/安装器说明 |
| Windows Cloud Files API | Windows + `UBAANEXT_ENABLE_CLOUD_FILES=ON` | Windows sync root frontend 预埋 | 属于 Windows SDK/API 使用；完成 adapter 后补充分发约束 |
| libfuse3 / libfuse | Linux + `UBAANEXT_ENABLE_FUSE=ON` 且 pkg-config 找到依赖 | Linux FUSE frontend 预埋 | 完成 Runtime 接入和分发包后补充目标系统包要求 |

UBAA Next 不应把第三方源码直接复制进 core。平台或协议所需能力应通过 CMake target 和 adapter 边界引入。当前真实协议与功能对照阶段暂不删除 `nlohmann-json`、`Catch2`、`curl/libcurl` 或 `OpenSSL`；依赖裁剪需在真实只读协议稳定并完成替代方案评估后单独处理。

如果新增依赖，需要同时更新：

1. `vcpkg.json` 或对应 CMake 查找逻辑。
2. `docs/build/dependency-policy.md`。
3. 本第三方声明。
4. 对应许可证或 NOTICE 条目（如依赖许可证要求）。
