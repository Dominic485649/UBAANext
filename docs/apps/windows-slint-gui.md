# Windows Slint 桌面应用

> 当前仓库版本号仍为 `0.4.0`，但 `master` 已包含实验性的 Slint 桌面入口。本文描述当前源码状态，不把尚未注册的 Cloud mount adapter 说成已可用能力。

## 当前入口

- CMake target：`UBAANextDesktop`
- 源码目录：`apps/desktop/`
- UI 文件：`apps/desktop/ui/main_window.slint`
- Windows 输出：`bin/<arch>/<config>/ubaa.exe`
- CLI 输出：`bin/<arch>/<config>/ubaa.com`

Windows 下同名 `ubaa` 使用 `.com` / `.exe` 分工：

- `ubaa.com` 是命令行应用，供终端、脚本、smoke test 和 `ubaa <command>` 使用。
- `ubaa.exe` 是 Slint 桌面 UI。双击或直接运行 `.exe` 会打开 GUI；`ubaa.exe --help` 会提示使用 `ubaa.com` 执行 CLI 命令。

## 构建

桌面目标默认关闭，需要显式启用：

```powershell
cmake --fresh --preset windows-ninja-msvc-debug -DUBAANEXT_BUILD_DESKTOP=ON
cmake --build --preset windows-ninja-msvc-debug --target UBAANextDesktop ubaa
```

Slint 通过 `find_package(Slint CONFIG QUIET)` 查找。若本机没有安装 Slint，可在明确接受 CMake 配置阶段访问网络时加 `-DUBAANEXT_FETCH_DEPS=ON`，当前 CMake 会 FetchContent 拉取 Slint `v1.16.1`。

## 运行和诊断

```powershell
.\bin\x64\Debug\ubaa.exe --version
.\bin\x64\Debug\ubaa.exe --diagnose
.\bin\x64\Debug\ubaa.exe --mock
```

GUI 当前覆盖的真实功能边界：

- 创建共享 `RuntimeContext`，复用 CLI 的 platform context、cookie/session 路径、HTTP/Crypto adapter 和 CloudService。
- 显示版本、模式、平台能力、app data/cache 路径和诊断 JSON。
- `Refresh Cloud` 会通过 `CloudVfs` 加载个人云盘根目录并列出 `/`。
- `Clear Cache` 会清理 Cloud VFS 内容缓存和 Runtime cache。
- mount 区域已有 WinFsp、Cloud Files、Linux FUSE 按钮，但当前 Runtime 没有注册对应 `ICloudMountAdapter`，所以 Windows GUI 只能显示依赖或 adapter 不可用的状态，不能实际挂载盘符或 sync root。

## 可选平台能力

相关 CMake 开关：

| 选项 | 默认值 | 当前作用 |
| --- | --- | --- |
| `UBAANEXT_BUILD_DESKTOP` | `OFF` | 构建 `UBAANextDesktop` Slint 应用 |
| `UBAANEXT_FETCH_DEPS` | `OFF` | 缺少 Slint 时允许 FetchContent 下载 |
| `UBAANEXT_ENABLE_WINFSP` | `OFF` | 查找 WinFsp SDK 并暴露 capability；当前无 Windows adapter 源文件注册 |
| `UBAANEXT_ENABLE_CLOUD_FILES` | `OFF` | 查找 `cfapi.h` / `CfApi.lib` 并暴露 capability；当前无 adapter 源文件注册 |

WinFsp 与 Cloud Files 仍是 mount frontend 预埋点，不是已完成的 Windows 挂载实现。发布说明、截图和 smoke 结果必须写明是否只是 `--diagnose` 能力探测。

## 依赖

基础构建依赖与 CLI 相同：CMake、C++ 编译器、Ninja 或 Visual Studio generator、vcpkg manifest 中的 `nlohmann-json`、`curl`、`openssl`、`catch2`。桌面额外需要 Slint C++ 包；通过 FetchContent 获取 Slint 时还需要 Git、Rust/Cargo 和可访问 GitHub 的网络环境。

## 许可注意

Slint 不在 `vcpkg.json` 的固定依赖列表中，只有构建桌面目标时才需要。分发 GUI 前必须按实际采用的 Slint 授权模式更新第三方声明、打包 NOTICE 和界面署名；当前文档只记录工程集成状态，不构成最终许可选择。
