# 第三方声明

本文档记录 UBAA Next 当前直接声明的第三方依赖。完整版本以仓库根目录 `vcpkg.json` 和实际构建解析结果为准。

| 依赖 | 用途 | 许可证 |
| --- | --- | --- |
| nlohmann-json | JSON 解析与序列化 | MIT License |
| Catch2 | 单元测试与集成测试框架 | BSL-1.0 |
| curl / libcurl | 真实 HTTP 网络栈 | curl license |
| OpenSSL | 加密 provider | Apache License 2.0 |

UBAA Next 不应把第三方源码直接复制进 core。平台或协议所需能力应通过 CMake target 和 adapter 边界引入。当前真实协议与功能对照阶段暂不删除 `nlohmann-json`、`Catch2`、`curl/libcurl` 或 `OpenSSL`；依赖裁剪需在真实只读协议稳定并完成替代方案评估后单独处理。

如果新增依赖，需要同时更新：

1. `vcpkg.json`
2. `docs/build/dependency-policy.md`
3. 本第三方声明
4. 对应许可证或 NOTICE 条目（如依赖许可证要求）
