# Cloud mount 设计与当前边界

> 本页描述 `master` 当前 Cloud VFS / mount Runtime 设计。WinFsp、Cloud Files 和 FUSE 在 UI 与 capability 中已有预埋，但除 Linux 只读 FUSE 低层封装源码外，尚未形成可由 Runtime 注册并启动的完整 adapter。

## 目标分层

```
CloudService
  ↑
CloudVfs::CloudVfs
  ↑
Runtime::CloudMountManager
  ↑
ICloudMountAdapter / ICloudMountSession
  ↑
WinFsp / Cloud Files / FUSE frontend
```

- `CloudService` 负责北航云盘协议命令，复用 Core 的 HTTP、Cookie、Cache 和上传抽象。
- `CloudVfs::CloudVfs` 把云盘节点抽象成路径树，提供 lookup/list/read/writeback/task/cache 能力。
- `CloudMountManager` 管理 frontend 状态、互斥写挂载和 start/stop/statuses API。
- `ICloudMountAdapter` 是平台挂载前端适配点，实际 WinFsp、Cloud Files、FUSE 启动逻辑必须通过它注册到 Runtime。

## 当前实现状态

| frontend | 代码状态 | Runtime 状态 | 用户可见结果 |
| --- | --- | --- | --- |
| WinFsp | 仅 CMake SDK 探测和 capability 开关；未发现 Windows adapter 源文件 | 未注册 adapter | GUI 点击 Start WinFsp 会报告不可用或 unsupported |
| Cloud Files | 仅 `cfapi.h` / `CfApi.lib` 探测和 capability 开关；未发现 adapter 源文件 | 未注册 adapter | GUI 点击 Start Cloud Files 会报告不可用或 unsupported |
| Linux FUSE | `platform/linux/src/LinuxFuseMount.cpp` 有只读 low-level FUSE 封装 | 未注册 adapter | `ubaa-gui --diagnose` 可显示 FUSE capability，但 GUI 不能实际挂载 |

`RuntimeContext` 当前会创建 `CloudService` 与只读 `CloudVfs`，但没有调用 `CloudMountManager::set_vfs()` 或 `register_adapter()`。因此 mount 区域应视为前端占位和诊断入口，不是可交付挂载功能。

## 写能力策略

Cloud VFS 与 CLI 的云盘写命令已经有写 gate 约束：

- CLI 写命令必须逐次传入 `--confirm` / `--yes` / `-y` 或在人类可读模式输入 `y`。
- live/cloud smoke 的可逆写阶段还需要 `UBAANEXT_ALLOW_WRITE=1` 和 `UBAANEXT_CONFIRM_WRITE=1`。
- `CloudMountManager` 设计上同一账号只允许一个 writable frontend；后启动的 writable request 会被降级为只读。

当前未完成真实 mount adapter 前，不应宣称桌面端支持从系统文件管理器写回云盘。

## 依赖和 CMake 开关

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `UBAANEXT_BUILD_DESKTOP` | `OFF` | 构建 Slint 桌面 UI 和 mount 按钮 |
| `UBAANEXT_ENABLE_WINFSP` | `OFF` | Windows 下查找 WinFsp SDK/runtime 依赖 |
| `UBAANEXT_ENABLE_CLOUD_FILES` | `OFF` | Windows 下查找 Cloud Files API 依赖 |
| `UBAANEXT_ENABLE_FUSE` | `OFF` | Linux 下查找 `fuse3` 或 `fuse` |

这些开关只控制依赖探测和部分编译能力，不等价于完整 adapter 已接入。

## 验证建议

当前可验证：

```powershell
.\bin\x64\Debug\ubaa.exe --diagnose
```

```bash
./bin/x64/Debug/ubaa-gui --diagnose
```

应记录：

- `capabilities.winfsp`、`capabilities.cloudFiles`、`capabilities.fuse` 的探测结果。
- `mounts[]` 是否为空或显示不可用状态。
- 点击 GUI mount 按钮时返回的脱敏错误消息。

当前应跳过：

- Windows 盘符实际挂载。
- Windows Cloud Files sync root 注册。
- Linux 用户目录实际 FUSE mount。
- 通过文件管理器执行云盘写回。

跳过原因：Runtime 尚未注册对应 `ICloudMountAdapter`，Windows adapter 源文件缺失，Linux FUSE 封装尚未接入 CloudMountManager。
