# UBAA Next

UBAA Next 是 UBAA（智慧北航 Remake）的 C++ 原生重写版本，面向北京航空航天大学学生提供校园服务聚合能力。

## 当前状态

**v0.4 CLI 已完成**：命令行入口、JSON 输出、配置管理、Mock 离线数据和集成测试已可用。真实校园 API 已接入部分流程，BYXT 会话激活仍需要继续打磨。

## 构建要求

- CMake 3.21+
- Ninja 或 Visual Studio 生成器
- Windows：MSVC 编译器，可配合 vcpkg 安装 `nlohmann-json` 与 `catch2`
- OpenHarmony：DevEco Studio Native SDK；设置 `OHOS_NATIVE_HOME` 指向 SDK 的 `openharmony/native` 目录

项目默认使用 C++17，可通过 `UBAANEXT_CXX_STANDARD=20` 验证 C++20 构建。

## Windows 构建与测试

推荐先执行一次 configure；修改过 preset 或 CMake 选项时，优先使用 `--fresh` 重新生成构建目录。静态 Release 建议在 Visual Studio Developer Command Prompt 中执行。

### Debug CLI

```powershell
cmake --fresh --preset windows-ninja-msvc-debug
cmake --build --preset windows-ninja-msvc-debug --target ubaa
ctest --preset windows-ninja-msvc-debug
```

### Release CLI

```powershell
cmake --fresh --preset windows-ninja-msvc-release
cmake --build --preset windows-ninja-msvc-release --target ubaa
```

如果只是重复编译、且未修改 preset/CMake 配置，也可以直接执行：

```powershell
cmake --build --preset windows-ninja-msvc-debug --target ubaa
cmake --build --preset windows-ninja-msvc-release --target ubaa
```

如果需要验证 C++20：

```powershell
cmake --preset windows-ninja-msvc-debug -DUBAANEXT_CXX_STANDARD=20
cmake --build --preset windows-ninja-msvc-debug
ctest --preset windows-ninja-msvc-debug
```

## OpenHarmony Native 构建

OpenHarmony 预设使用 SDK 自带 `ohos.toolchain.cmake`，默认目标 ABI 为 `arm64-v8a`，并关闭测试目标。运行前设置 `OHOS_NATIVE_HOME`，例如让它指向 DevEco SDK 中的 `openharmony/native` 目录。

```powershell
$env:OHOS_NATIVE_HOME="<DevEco SDK>/openharmony/native"
$env:UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR="<包含 nlohmann 文件夹的 include 路径>"
cmake --preset openharmony-clang-debug
cmake --build --preset openharmony-clang-debug
```

当前 OpenHarmony 预设不再假设某台 Windows 机器上的 vcpkg 安装位置；如果依赖位置不同，请通过 `UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR` 或 `-DUBAANEXT_NLOHMANN_JSON_INCLUDE_DIR=...` 指定。

## CLI 使用

### Mock 离线模式

```powershell
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe version --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe help --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe login --mock --username 20260000 --password test --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe whoami --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course today --mock --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course week --mock --week 8 --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe exam list --mock --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe classroom query --mock --campus 1 --date 2026-05-13 --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe term list --mock --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe week list --mock --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe config show --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe cache clear --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe logout --json
```

### 真实模式（VPN/WebVPN）

```powershell
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe login --username <student_id> --password <password> --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course today --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course date --date 2026-05-13 --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course week --week 8 --mode vpn --json
```

## 常用命令

| 命令 | 说明 |
| --- | --- |
| `version [--json]` | 显示版本 |
| `help [--json]` | 显示帮助 |
| `login [--mock] --username <id> --password <pw> [--mode vpn/direct]` | 登录 |
| `whoami [--json]` | 显示当前用户 |
| `logout [--json]` | 登出 |
| `course today [--mock] [--mode vpn/direct] [--json]` | 显示今日课程 |
| `course date --date <yyyy-MM-dd> [--mock] [--mode vpn/direct] [--json]` | 显示指定日期课程 |
| `course week --week <n> [--mock] [--mode vpn/direct] [--json]` | 显示指定周课程 |
| `exam list [--mock] [--mode vpn/direct] [--json]` | 显示考试 |
| `classroom query --campus <id> --date <yyyy-MM-dd> [--mock] [--mode vpn/direct] [--json]` | 查询空闲教室 |
| `term list [--mock] [--mode vpn/direct] [--json]` | 显示学期 |
| `week list [--mock] [--mode vpn/direct] [--json]` | 显示教学周 |
| `config show [--json]` | 显示配置 |
| `config set --key <key> --value <value> [--json]` | 设置配置 |
| `cache clear [--json]` | 清除缓存 |

## 退出码

| 退出码 | 含义 |
| --- | --- |
| 0 | 成功 |
| 1 | 通用失败 |
| 2 | 参数无效 |
| 3 | 需要认证 |
| 4 | 网络错误 |
| 5 | 解析错误 |
| 6 | 存储错误 |

## 文档

中文文档入口见 [docs/README.md](docs/README.md)。

## 许可证

MIT License。详见 [LICENSE](LICENSE) 与 [NOTICE](NOTICE)。
