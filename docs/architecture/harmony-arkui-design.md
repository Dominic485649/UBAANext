# Harmony ArkUI 设计

## 当前范围

当前阶段仅验证 `UBAANextCore` 与 CLI 逻辑在 OpenHarmony Native 工具链下可编译，不迁移 ArkUI 页面，也不补齐 Direct/WebVPN 登录能力。

## Native 边界原则

- ArkTS/NAPI 层只依赖稳定的 Core 服务语义，不直接拼接校园系统协议。
- Core 输出优先使用强类型模型；暂未强类型化的高级服务保持 `FeatureRecord { id, title, status, fields }` 兼容结构。
- 写操作必须延续 CLI 的 `--confirm` 等价安全门，由 ArkUI 显式二次确认后才调用 Core mutation。
- 非 Windows 平台不依赖 WinHTTP、DPAPI、BCrypt；BYKC、LibBook、CGYY 等加密能力后续应统一收敛到平台 `ICryptoProvider`。

## 已验证命令

```powershell
cmake --build "build\\openharmony-clang-debug" --config Debug
```

该构建用于发现非 Windows 分支的 `-Werror` 问题和头文件可移植性问题。当前 CLI 仍是验证载体，后续 NAPI 模块应复用同一套 Core 头文件与服务构造方式。

## ArkUI 迁移建议

- 首页/功能页按 CLI 命令矩阵映射：课程、考试、成绩、教室、SPOC、Judge、签到、博雅、场馆、图书馆、打卡、评教、待办。
- 所有列表页面先消费 JSON 等价字段：强类型服务直接映射字段；`FeatureRecord` 服务用 `fields` 承载扩展字段。
- 登录状态、Cookie、缓存应在 Native 侧统一管理，ArkUI 只展示状态和触发动作。
- 真实网络和加密能力完成平台抽象前，Harmony 端优先使用 mock/offline 与只读兼容性验证。
