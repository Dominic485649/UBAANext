# Windows Slint 图形界面集成与编译指南 (v0.8 计划)

> 当前仓库版本阶段为 `v0.3.0`，Slint GUI 属于路线图 `v0.8 — Windows Slint GUI` 后续计划。本页是设计与编译方案草案，不代表 v0.3 当前可交付能力。

UBAA Next 的 Windows 桌面图形界面（GUI）计划采用声明式现代 UI 框架 **Slint** 进行集成开发。遵循“**C++ 核心 + 平台外壳**”的设计架构，GUI 部分作为一个轻量级的视图表现层，通过 `ViewModel` 模式调用稳定的 `UBAANextCore` 库。这种解耦方式保证了核心业务逻辑的纯净，也极大地简化了 GUI 层的构建和打包过程。

---

## 1. 桌面客户端集成架构设计

在架构层面上，Windows Slint 应用程序（`apps/windows-slint`）被设计为 `UBAANextCore` 的一个独立下游依赖项。

### 1.1 核心解耦设计原则
1. **核心不依赖 UI (Core-Independent)**：`UBAANextCore` 库绝对禁止包含任何关于 Slint、Qt、Windows API 等 UI 相关的头文件或依赖。
2. **纯粹的数据绑定**：GUI 界面与底层 C++ 服务之间只通过 `ViewModel` 进行通信，UI 层接收强类型数据结构，并将用户交互操作转化为 C++ 核心库的服务调用。
3. **依赖分离**：Slint 的 CMake 编译流程及第三方 Rust 依赖项被限定在 `apps/windows-slint` 的构建树内，不影响命令行工具（CLI）和核心静态库的轻量化。

### 1.2 架构模块调用关系

```
┌─────────────────────────────────────────────────────────┐
│              Windows Slint GUI (外壳应用)               │
│  ┌───────────────────────┐   ┌───────────────────────┐  │
│  │   Slint UI (.slint)   │   │  C++ ViewModel / App  │  │
│  │ 声明式界面、动画、交互事件│ ◄─┼─► 数据模型转换与属性绑定  │  │
│  └───────────────────────┘   └───────────┬───────────┘  │
└──────────────────────────────────────────┼──────────────┘
                                           ▼
┌─────────────────────────────────────────────────────────┐
│                   UBAA Next Core 核心库                 │
│  ┌───────────────────────┐   ┌───────────────────────┐  │
│  │      Service 层       │   │      Adapter 层       │  │
│  │   Course/Auth 等服务   │ ──►│   DPAPI 安全存储/Curl  │  │
│  └───────────────────────┘   └───────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 编译前置依赖与工具链要求

在 Windows 系统上成功编译含有 Slint GUI 的项目，必须预先安装以下软件开发套件：

### 2.1 基础构建工具链
*   **C++ 编译器**：支持 C++17 或更高标准。推荐使用 **Microsoft Visual C++ (MSVC) 2022 (v17.0+)**。
*   **CMake 构建系统**：版本要求 **CMake 3.26** 或更高版本，以便良好地支持 vcpkg 清单模式及 Slint CMake 自动生成规则。
*   **构建生成器**：推荐使用 **Ninja** 以获得最快的编译速度，或者选择 Visual Studio 17 2022 生成器。

### 2.2 核心外部依赖管理
*   **Rust 工具链**：Slint 框架的核心渲染引擎及编译器主要由 Rust 编写。构建项目前必须安装 **Rustc & Cargo (v1.75+)** 并确保其所在的 `bin` 路径已加入系统的 `PATH` 环境变量中。
*   **Vcpkg 依赖管理器**：用于统一拉取 C++ 端的常规三方库。项目通过 `vcpkg.json` 清单管理依赖，需要拉取以下库：
    *   `nlohmann-json`（C++ JSON 解析库）
    *   `openssl`（真实网络 TLS 支持）
    *   `curl`（真实 HTTP 传输底座，编译静态版 `x64-windows-static`）
    *   `catch2`（测试框架）

---

## 3. CMake 关键编译变量与配置项

可以通过向 CMake 传入特定的变量，以控制是否构建 Slint GUI 及其内部行为：

| CMake 配置项参数 | 默认值 | 描述 |
| :--- | :--- | :--- |
| **`UBAANEXT_BUILD_GUI`** | `OFF` | **Slint GUI 构建主开关**。若要构建 Windows Slint 应用程序，必须显式将其设置为 `ON`。 |
| **`UBAANEXT_CXX_STANDARD`**| `17` | 指定 C++ 编译标准。支持 `"17"` 或 `"20"`、`"23"`。建议在 Windows GUI 上使用稳定版标准。 |
| **`UBAANEXT_ENABLE_MOCKS`**| `ON` | 决定是否在构建中包含 Mock 测试控制器及本地离线数据链路，便于开发阶段免联网调试 UI。 |
| **`CMAKE_TOOLCHAIN_FILE`** | 无 | 必须指定为本地 vcpkg 路径（例如 `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`）来接管包依赖。 |
| **`VCPKG_TARGET_TRIPLET`** | `x64-windows` | 静态或动态链接三方库规格。对于正式发布，建议使用 `x64-windows-static` 编译单一、无 dll 依赖的绿色 GUI 应用程序。 |

---

## 4. 详细的编译与装配指南

以下是在 Windows 环境中，使用命令行工具克隆、配置、编译并装配运行 UBAA Next Slint 桌面程序的完整过程。

### 4.1 环境初始化
打开 PowerShell，执行以下命令以确认环境依赖可用：

```powershell
# 验证 C++ 编译器及 CMake 版本
cmake --version
cl

# 验证 Rust 工具链
rustc --version
cargo --version
```

### 4.2 配置 CMake 构建系统
建议使用项目内置的 CMake 预设（CMake Presets），这能避免手动输入复杂的绝对路径。

使用 **Ninja + MSVC** 的命令行调试构建配置：
```powershell
# 使用内置 preset 结合 GUI 构建开关进行配置
cmake --preset windows-ninja-msvc-debug -DUBAANEXT_BUILD_GUI=ON
```

如果未配置 Presets，可以手动配置，指定本地的 `VCPKG_ROOT`：
```powershell
# 手动创建并进入 build 目录
mkdir build
cd build

# 执行 CMake 全局配置，激活 GUI 编译
cmake .. -G "Ninja" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DUBAANEXT_BUILD_GUI=ON `
  -DCMAKE_BUILD_TYPE=Debug
```

### 4.3 编译 Slint GUI 应用
配置完成后，直接通过 CMake 执行跨生成器的编译指令：

```powershell
# 开始编译 GUI 目标文件（将自动触发 Rust 端的 Slint C++ 绑定代码生成）
cmake --build . --target windows-slint
```

编译过程中，CMake 的 `slint_target_sources` 宏会调用本地的 Rust 编译器编译 UI 描述文件（`.slint`），生成对应的 C++ 派生类头文件，然后与 [apps/windows-slint/src/main.cpp](file:///d:/Code/Cpp/UBAANext/apps/windows-slint/src/main.cpp) 进行链接。

### 4.4 运行及装配
编译生成的 Windows 绿色执行程序位于：
`build/apps/windows-slint/windows-slint.exe`

为了使得应用能够独立脱离开发环境运行，在打包装配时需注意：
1. **静态编译 (Static Triplet)**：如果使用 `x64-windows-static` 编译，`windows-slint.exe` 会将 `libcurl`、`openssl` 和核心依赖全部静态嵌入，此时直接双击运行即可。
2. **动态编译 (Dynamic Triplet)**：如果是普通的 `x64-windows` 动态编译，必须将 `libcurl.dll`、`libcrypto-3-x64.dll`、`libssl-3-x64.dll` 以及相关的 C++ 运行时动态库复制到与 `windows-slint.exe` 同级的安装目录下。
