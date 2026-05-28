# UBAA Next

UBAA Next 是 UBAA（智慧北航 Remake）的 C++ 原生重写版本，面向北京航空航天大学学生提供校园服务聚合能力。

## 当前状态

当前代码库以 **C++ Core + Platform Shell** 为主线：

- `UBAANextCore` 承载业务模型、协议解析、认证/session、服务层和网络/存储抽象。
- `platform/common/curl` 与 `platform/common/openssl` 提供真实网络和加密适配。
- `platform/windows`、`platform/linux`、`platform/harmony` 提供平台能力、安全存储和路径等边界实现。
- Windows CLI 已覆盖 mock 离线模式、真实登录/session、JSON 输出、配置管理和多业务服务入口。

真实校园 API 已接入部分流程；不同业务域的真实协议成熟度不一致，涉及账号或写操作的验证必须显式启用，默认构建和测试仍以离线回归为主。能力状态统一使用 `Aligned`、`ReadOnlyCandidate`、`PartiallyMigrated`、`MockOnly`、`Placeholder`、`NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified`。CLI 暴露、service 类存在或 mock/offline 测试通过都不代表原 UBAA 后端语义已完成。

真实写操作必须同时通过 typed service、CLI/API 显式确认和平台 `write_operations` capability；默认平台能力关闭真实写，默认 live smoke 不执行任何会改变远端状态的命令。secure store、cookie/session persistence、crypto 或 live login 能力不可用时必须 fail-closed，不能回退成明文、volatile 或假成功。

## 构建要求

- CMake 3.21+
- Ninja 或 Visual Studio 生成器
- vcpkg manifest 依赖：`nlohmann-json`、`catch2`、`openssl`、`curl`
- Windows：MSVC 编译器，推荐配合 vcpkg toolchain
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

如果依赖位置不同，请通过 `UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR` 或 `-DUBAANEXT_NLOHMANN_JSON_INCLUDE_DIR=...` 指定。

## CLI 使用

### 能力状态与安全边界

- `file upload --path <path> --confirm` 是 `Placeholder` / `NotImplemented` 占位接口：确认后稳定失败，不读取本地文件，不触发远端请求。
- `ygdk submit --photo <path>` 是 `WriteGated` typed 写上传场景：会读取本地文件，可能上传图片并改变远端打卡状态，默认平台 `write_operations=false` 时必须失败。
- 真实登录和 live 只读请求可能保存 cookie/session；平台 secure store 或 cookie persistence 不可用时必须拒绝真实持久化。
- 错误、日志、diagnostics 和测试输出不得泄露 username、password、cookie、token、ticket、session、captcha、authorization、上传文件名、上传 bytes、敏感 URL query 或 raw HTML。

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
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe cache clear --confirm --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe logout --confirm --json
```

### 真实模式（VPN/WebVPN）

```powershell
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe login --username <student_id> --password <password> --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course today --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course date --date 2026-05-13 --mode vpn --json
.\build\windows-ninja-msvc-debug\apps\cli\ubaa.exe course week --week 8 --mode vpn --json
```

不要在普通回归中运行需要真实账号、密码或线上写操作的命令。写操作命令必须显式传入 `--confirm` 或 `--yes`，并且当前平台必须声明 `write_operations=true`；默认平台能力关闭真实写操作。真实登录、cookie/session 保存和 live 只读都受平台 capability 与错误脱敏约束；能力缺失时应返回稳定错误，而不是隐式降级成功。

## 常用命令

| 命令 | 说明 |
| --- | --- |
| `version [--json]` | 显示版本 |
| `help [--json]` | 显示帮助 |
| `login [--mock] --username <id> --password <pw> [--mode vpn/direct]` | 登录 |
| `whoami [--json]` | 显示当前用户 |
| `logout --confirm [--json]` | 登出并清除本地会话 |
| `course today [--mock] [--mode vpn/direct] [--json]` | 显示今日课程 |
| `course date --date <yyyy-MM-dd> [--mock] [--mode vpn/direct] [--json]` | 显示指定日期课程 |
| `course week --week <n> [--mock] [--mode vpn/direct] [--json]` | 显示指定周课程 |
| `exam list [--mock] [--mode vpn/direct] [--json]` | 显示考试 |
| `classroom query --campus <id> --date <yyyy-MM-dd> [--mock] [--mode vpn/direct] [--json]` | 查询空闲教室 |
| `term list [--mock] [--mode vpn/direct] [--json]` | 显示学期 |
| `week list [--mock] [--mode vpn/direct] [--json]` | 显示教学周 |
| `grade list [--all] [--term <term>] [--mock] [--json]` | 查询成绩；敏感只读输出 |
| `grade all [--mock] [--json]` | 查询全部成绩的 CLI 验收入口 |
| `todo list [--pending-only|--all] [--mock] [--json]` | 查询聚合待办，可能包含来源级失败记录 |
| `ygdk records [--page <n>] [--size <n>|--limit <n>] [--mock] [--json]` | 查询阳光打卡记录 |
| `file upload --path <path> --confirm [--json]` | 占位上传接口，确认后返回 `NotImplemented` |
| `config show [--json]` | 显示配置 |
| `config set --key <key> --value <value> --confirm [--json]` | 设置配置 |
| `cache clear --confirm [--json]` | 清除缓存 |

更多业务命令见 `ubaa help`。

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

中文文档入口见 [docs/README.md](docs/README.md)。CLI contract 见 [docs/api/cli-command-api.md](docs/api/cli-command-api.md)，原 UBAA 后端语义差异见 [docs/reports/original-ubaa-backend-feature-matrix.md](docs/reports/original-ubaa-backend-feature-matrix.md)，测试策略见 [docs/testing/test-strategy.md](docs/testing/test-strategy.md)。Core 跨平台性、代码质量和保守清理结论见 [docs/reports/core-portability-and-cleanup-report.md](docs/reports/core-portability-and-cleanup-report.md)。

## 许可证

MIT License。详见 [LICENSE](LICENSE) 与 [NOTICE](NOTICE)。
