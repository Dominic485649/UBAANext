# Windows 静态二进制打包与 vcpkg 集成编译指南

> 当前仓库版本阶段为 `v0.3.0`。本页描述 v0.4+ / v1.0 发布前的 Windows 打包目标形态；v0.3 当前基线不承诺已经具备正式静态 Release 分发包。

本文档详尽论述了 UBAA Next 项目在 Windows 平台上的编译构建系统、vcpkg 依赖管理系统的集成方式，以及通过静态链接（Static Linking）机制输出独立绿色版命令行客户端 `ubaa.exe` 的完整技术方案。

---

## 1. Windows 构建系统架构

UBAA Next 在 Windows 平台上使用 **CMake (3.21+)** 作为构建元系统，配合 **Ninja** 或 **Visual Studio** 作为底层生成器，编译器推荐采用 **Microsoft Visual C++ (MSVC)**。

### 1.1 构建预设（CMake Presets）
在项目的 [`CMakePresets.json`](file:///d:/Code/Cpp/UBAANext/CMakePresets.json) 中，已预设了面向 Windows 平台的标准化配置流。最常用的两个预设为：
* **`windows-ninja-msvc-debug`**：使用 Ninja 生成器和 MSVC 编译器进行 Debug 构建，默认启用单元测试（`UBAANEXT_BUILD_TESTS=ON`）和 Mock 离线组件（`UBAANEXT_ENABLE_MOCKS=ON`）。
* **`windows-ninja-msvc-release`**：进行 Release 构建，优化级别为 `-O2`，剥离调试符号，关闭或限制部分开发期特有的 Mock 逻辑（`UBAANEXT_ENABLE_MOCKS=OFF`）。

---

## 2. vcpkg 依赖管理清单（Manifest Mode）

本项目使用 vcpkg 的**清单模式（Manifest Mode）**对外部三方库进行统一拉取和版本对齐。

### 2.1 清单文件结构
项目根目录下的 [`vcpkg.json`](file:///d:/Code/Cpp/UBAANext/vcpkg.json) 声明了编译所需的直接依赖项：
```json
{
  "name": "ubaanext",
  "version-string": "0.3.0",
  "dependencies": ["nlohmann-json", "catch2", "openssl", "curl"]
}
```
* **nlohmann-json**：用于协议数据的快速序列化与反序列化，为 Header-only。
* **curl**：核心网络栈，提供高并发 HTTP(S) 请求、Cookie 持久化支持。
* **openssl**：底层加密套件，配合 curl 确保 HTTPS 连接的 SSL 握手及会话加解密安全。
* **catch2**：现代化 C++ 单元测试框架（仅在启用测试构建时编译）。

### 2.2 构建期间的依赖解析
当 CMake 运行时，若传入了 vcpkg 工具链文件（`-DCMAKE_TOOLCHAIN_FILE=...`），vcpkg 会自动扫描并读取 `vcpkg.json`，在本地的构建目录下（如 `vcpkg_installed/`）下载、编译并安装这些依赖。
在 [`cmake/Dependencies.cmake`](file:///d:/Code/Cpp/UBAANext/cmake/Dependencies.cmake) 中，使用标准的 `find_package` 去检索这些预先安装好的包：
```cmake
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
```

---

## 3. 独立绿色二进制：纯静态链接技术方案

为了让最终交付的命令行客户端 `ubaa.exe` 能够“即开即用”，避免因用户操作系统缺失特定动态库（如 `libcurl.dll`、`ssleay32.dll`）而导致程序崩溃，项目采用**全静态链接（Static Linking）**进行 Release 打包。

### 3.1 核心配置要素
1. **静态三方库下载（Static Triplet）**：
   在配置 CMake 时，将 vcpkg 的目标架构三方库类型（Triplet）指定为静态。例如，使用 **`x64-windows-static`**。这会强制 vcpkg 将 `CURL`、`OpenSSL` 分别编译为静态库（`.lib`），以便其目标代码直接嵌入最终的 `ubaa.exe`。
   命令行配置参数：
   ```powershell
   -DVCPKG_TARGET_TRIPLET=x64-windows-static
   ```
2. **C/C++ 运行时库（MSVC CRT）静态链接**：
   在 MSVC 下，默认编译会动态链接到系统的 `msvcrt.dll` 或 `vcruntime140.dll`。为了彻底消除这一外部依赖，需要在 CMake 中将运行时库链接方式设为静态（`MultiThreaded` 模式）：
   ```cmake
   if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
       set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
   endif()
   ```
   * **Release 配置**：对应 MSVC 编译选项 `/MT`（静态链接多线程 Release 运行时）。
   * **Debug 配置**：对应 MSVC 编译选项 `/MTd`（静态链接多线程 Debug 运行时）。

---

## 4. 编译与打包执行命令指南

推荐在 **Visual Studio Developer Command Prompt** 终端中运行以下命令，以确保 MSVC 编译器（`cl.exe`）环境路径被正确识别：

### 4.1 Debug 开发编译与单元测试
```powershell
# 1. 强制全新 configure 并使用 Debug 预设
cmake --fresh --preset windows-ninja-msvc-debug

# 2. 编译 CLI 目标 ubaa
cmake --build --preset windows-ninja-msvc-debug --target ubaa

# 3. 运行本地单元测试
ctest --preset windows-ninja-msvc-debug
```

### 4.2 Release 静态独立包发布编译
```powershell
# 1. 配置静态 Release 预设（指定使用静态 triplet 覆盖默认动态链接）
cmake -S . -B build/windows-static-release `
  -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Release `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DCMAKE_TOOLCHAIN_FILE="D:\Code\vcpkg\scripts\buildsystems\vcpkg.cmake" # 请替换为实际本地 vcpkg 路径

# 2. 构建最终独立二进制
cmake --build build/windows-static-release --target ubaa --config Release
```

---

## 5. 打包制品完整性校验说明

在构建完成后，必须对生成的 `ubaa.exe` 独立性进行严格的**动态依赖审计（Dependency Audit）**，确保没有任何“漏网之鱼”动态链接库存在。

### 5.1 使用 `dumpbin` 查看依赖
在 Developer Command Prompt 中对生成的 `ubaa.exe` 执行以下命令：
```powershell
dumpbin /dependents .\build\windows-static-release\apps\cli\ubaa.exe
```

### 5.2 预期输出指标（绿灯通过状态）
在 `/dependents` 列表输出中，应当**仅包含 Windows 操作系统自带的基础核心 DLL**：
* `KERNEL32.dll`
* `USER32.dll`
* `WS2_32.dll` （Windows 嵌套套接字服务）
* `ADVAPI32.dll` / `CRYPT32.dll` （Windows 底层安全与加密服务）
* `shell32.dll`

**绝对红线指标（红灯拦截状态）**：
如果在依赖列表中发现了以下任何一个 DLL 文件，则说明静态链接配置失败，**该包体严禁对外公开发布**：
* ❌ `vcruntime140.dll` 或 `msvcp140.dll`（表明未启用 `/MT` 静态运行时链接）
* ❌ `libcurl.dll`（表明链接了动态版 curl）
* ❌ `libssl-3-x64.dll` 或 `libcrypto-3-x64.dll`（表明链接了动态版 OpenSSL）

---

通过本静态打包规范的严密执行，UBAA Next 在 Windows 平台交付的客户端将具备无可比拟的易用性与绿色纯净度，为终端师生提供极速、稳定的免安装使用体验。
