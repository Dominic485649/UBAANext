# UBAA Next

UBAA Next 是 UBAA（智慧北航 Remake）的 C++ 原生重写版本，面向北京航空航天大学学生提供校园服务聚合能力。

## 当前状态

**v0.4 CLI 已完成**：命令行入口、JSON 输出、配置管理、Mock 离线数据和集成测试已可用。真实校园 API 已接入部分流程，BYXT 会话激活仍需要继续打磨。

## 构建要求

- CMake 3.21+
- Ninja 或 Visual Studio 生成器
- Windows：MSVC 编译器，可配合 vcpkg 安装 `nlohmann-json` 与 `catch2`
- OpenHarmony：DevEco Studio Native SDK，使用 `D:/DevTools/DevEco Studio/sdk/default/openharmony/native/llvm/bin/clang++.exe`

项目默认使用 C++17，可通过 `UBAANEXT_CXX_STANDARD=20` 验证 C++20 构建。

## Windows 构建与测试

```powershell
cmake --preset windows-ninja-msvc-debug
cmake --build --preset windows-ninja-msvc-debug
ctest --preset windows-ninja-msvc-debug
```

如果需要验证 C++20：

```powershell
cmake --preset windows-ninja-msvc-debug -DUBAANEXT_CXX_STANDARD=20
cmake --build --preset windows-ninja-msvc-debug
ctest --preset windows-ninja-msvc-debug
```

## OpenHarmony Native 构建

OpenHarmony 预设使用 SDK 自带 `ohos.toolchain.cmake`，默认目标 ABI 为 `arm64-v8a`，并关闭测试目标。

```powershell
cmake --preset openharmony-clang-debug
cmake --build --preset openharmony-clang-debug
```

当前 OpenHarmony 预设默认复用 Windows vcpkg 已安装的 `nlohmann/json.hpp` 头文件路径；如果你的本地依赖位置不同，请配置：

```powershell
cmake --preset openharmony-clang-debug -DUBAANEXT_NLOHMANN_JSON_INCLUDE_DIR=<包含 nlohmann 文件夹的 include 路径>
```

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
