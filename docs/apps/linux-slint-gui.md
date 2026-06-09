# Linux Slint 桌面应用

> 当前仓库版本号仍为 `0.4.0`，但 `master` 已包含实验性的跨平台 Slint 桌面入口。Linux 桌面能力与 Windows 共用 `apps/desktop/`，本文只说明 Linux 运行方式和边界。

## 当前入口

- CMake target：`UBAANextDesktop`
- 源码目录：`apps/desktop/`
- UI 文件：`apps/desktop/ui/main_window.slint`
- Linux 输出名：`ubaa-gui`
- Linux CLI 输出名：`ubaa`

Linux 不使用 Windows 的 `.com` / `.exe` 分工：

- `ubaa` 是 CLI。
- `ubaa-gui` 是 Slint 桌面 UI。

## 构建

```bash
cmake --fresh --preset linux-ninja-debug -DUBAANEXT_BUILD_DESKTOP=ON
cmake --build --preset linux-ninja-debug --target UBAANextDesktop ubaa
```

Slint 通过 `find_package(Slint CONFIG QUIET)` 查找。若本机没有安装 Slint，可在明确接受配置阶段联网时加 `-DUBAANEXT_FETCH_DEPS=ON`，当前 CMake 会 FetchContent 拉取 Slint `v1.16.1`。

## 启动方式

构建后按实际 binary 位置启动；若 preset 使用默认 runtime 输出目录，可使用：

```bash
./bin/x64/Debug/ubaa-gui --version
./bin/x64/Debug/ubaa-gui --diagnose
./bin/x64/Debug/ubaa-gui --mock
```

如果构建系统或 generator 将 runtime 输出放到 build tree，请以 `cmake --build` 日志中的 `UBAANextDesktop` 输出路径为准。

## 当前功能边界

Linux GUI 当前与 Windows GUI 使用同一套 Runtime：

- 创建共享 `RuntimeContext`，复用 CLI 的 platform context、cookie/session 路径、HTTP/Crypto adapter 和 CloudService。
- 显示版本、模式、平台能力、app data/cache 路径和诊断 JSON。
- `Refresh Cloud` 会通过 `CloudVfs` 加载个人云盘根目录并列出 `/`。
- `Clear Cache` 会清理 Cloud VFS 内容缓存和 Runtime cache。
- FUSE 按钮已出现在 UI 中，但当前 Runtime 没有注册 `ICloudMountAdapter`，所以 GUI 不会实际挂载目录。

## Linux FUSE 状态

源码中存在 `platform/linux/src/LinuxFuseMount.cpp`，它实现了基于 libfuse 的只读 `lookup/getattr/readdir/open/read` 路径，并在 CMake 选项 `UBAANEXT_ENABLE_FUSE=ON` 且找到 `fuse3` 或 `fuse` 时编译依赖能力。但它尚未接入 Runtime 的 `CloudMountManager::register_adapter`，因此不能从 GUI 或通用 Runtime API 直接启动。

启用 FUSE 依赖探测：

```bash
cmake --fresh --preset linux-ninja-debug -DUBAANEXT_BUILD_DESKTOP=ON -DUBAANEXT_ENABLE_FUSE=ON
```

`ubaa-gui --diagnose` 只能说明编译/运行时是否探测到 FUSE 能力，不能证明 Cloud VFS 已挂载到用户目录。

## 依赖

基础依赖与 Linux CLI 相同：CMake、C++ 编译器、Ninja、vcpkg manifest 依赖或等价包、curl/OpenSSL runtime。桌面额外需要 Slint C++ 包；通过 FetchContent 获取 Slint 时还需要 Git、Rust/Cargo 和可访问 GitHub 的网络环境。FUSE 预埋能力需要系统安装 `libfuse3-dev` 或兼容的 `libfuse` 开发包。
