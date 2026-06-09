# Slint 桌面架构设计

> 当前仓库版本号仍为 `0.4.0`，但 `master` 已包含实验性的跨平台 Slint 桌面入口。本文描述当前 `apps/desktop/` 实现与未完成边界。

## 目标结构

```
┌──────────────────────────────────────────────┐
│ Slint 桌面 Shell                              │
│ apps/desktop/src/main.cpp                     │
│ apps/desktop/ui/main_window.slint             │
│                                              │
│ - GUI 生命周期                               │
│ - 用户操作回调                               │
│ - 诊断展示和轻量 ViewModel 映射              │
└───────────────────────┬──────────────────────┘
                        │
┌───────────────────────▼──────────────────────┐
│ UBAANextAppRuntime                            │
│ apps/runtime/                                 │
│                                              │
│ - RuntimeContext                              │
│ - CloudService / CloudVfs                     │
│ - CloudMountManager                           │
│ - CLI PlatformContext 复用                    │
└───────────────────────┬──────────────────────┘
                        │
┌───────────────────────▼──────────────────────┐
│ UBAANextCore + Platform adapters              │
│                                              │
│ - Core 保持平台无关                           │
│ - Windows/Linux adapter 提供安全存储、网络等  │
└──────────────────────────────────────────────┘
```

## 当前 CMake 和产物

- 构建开关：`UBAANEXT_BUILD_DESKTOP=ON`
- 目标名：`UBAANextDesktop`
- Windows 输出：`bin/<arch>/<config>/ubaa.exe`
- Linux 输出：`bin/<arch>/<config>/ubaa-gui`
- Windows CLI：`bin/<arch>/<config>/ubaa.com`
- Linux CLI：`ubaa`

桌面目标依赖 `UBAANextAppRuntime` 和 `Slint::Slint`。`UBAANextAppRuntime` 复用部分 CLI runtime 代码，但仍作为独立 target 供 CLI/GUI 共享应用级能力。

## 当前 UI 能力

`apps/desktop/src/main.cpp` 当前实现：

- `--version`：输出桌面版本。
- `--help`：说明桌面/CLI 分工。
- `--diagnose`：输出 Runtime 诊断 JSON。
- `--mock`：用 mock runtime 打开 GUI。
- GUI 展示版本、连接模式、账号摘要、cache 状态、Cloud rows、task rows、mount 状态和 diagnostics。
- `Refresh Cloud`：调用 `CloudVfs::load_user_root()` 和 `CloudVfs::list("/")`。
- `Clear Cache`：清理 Cloud VFS 内容缓存和 Runtime cache。
- mount 按钮：调用 `CloudMountManager::start()`，但当前没有注册 adapter，因此只能显示不可用/unsupported 结果。

## 线程与 UI 约束

当前 GUI 回调在 Slint 事件线程内同步调用 Runtime。后续接入真实长耗时操作时，应遵守：

- 不在 UI 线程执行长时间网络下载、视频合并或大文件上传。
- 后台线程不得直接改 Slint component 属性，必须回到 Slint event loop 更新 UI。
- Runtime 错误展示必须先经过 `Security::redact_sensitive_text`。
- Core 不能包含 Slint、Win32、POSIX、FUSE、Cloud Files 或 WinFsp 头文件。

## Cloud mount 未完成边界

`CloudMountManager` 已有 start/stop/statuses 和单账号 writable frontend 互斥设计，但当前 `RuntimeContext` 尚未注册任何 `ICloudMountAdapter`。因此：

- Windows WinFsp 和 Cloud Files 不是已完成能力。
- Linux FUSE low-level 封装源码存在，但未接入 Runtime。
- GUI mount 按钮不应作为通过验证的系统挂载能力宣传。

详细状态见 [Cloud mount 设计与当前边界](cloud-mount-design.md)。

## 后续接入顺序

1. 在平台层实现 WinFsp / Cloud Files / FUSE 的 `ICloudMountAdapter`。
2. 在 Runtime 创建时 `set_vfs()` 并按平台注册 adapter。
3. 为 mount start/stop/status 补充单元测试和平台条件测试。
4. 先验证只读 mount，再验证可逆写回。
5. GUI 再补登录、设置、任务和长任务进度 UI。
