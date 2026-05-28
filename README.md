# UBAA Next

UBAA Next 是 UBAA（智慧北航 Remake）的 C++ 原生重写版本，目标是把校园服务能力沉淀为可复用的跨平台核心，再由 Windows CLI、HarmonyOS、桌面 GUI 等外壳按阶段接入。

## 当前版本阶段

当前项目版本号为 **0.4.0**，以 [docs/02-roadmap.md](docs/02-roadmap.md) 的 `v0.4 — CLI 工程化` 为基线。

这表示当前稳定基线覆盖：

- C++ Core 骨架：`Result/Error`、Model、Net/Storage 抽象。
- Mock 实现：`MockHttpClient`、`MockSecureStore` 与离线数据流。
- 登录与会话基础：AuthService、SessionManager、CookieJar、SecureStore 适配边界。
- 数据解析与缓存：`nlohmann/json`、课程/考试/教室/学期/教学周解析、MemoryCacheStore TTL。
- Windows CLI 工程化：稳定命令树、`CommandHandlers` 命令目录、统一 `--json` envelope、固定 exit code `0-6`、`config` / `cache clear` 子命令。
- CLI integration/golden tests：覆盖 help 合同、JSON 输出、exit code、写操作确认门与关键 mock/offline 命令。
- 单元测试与文档结构。

v0.4 仍保持默认离线、mock/offline 优先的稳定边界。真实 HTTP、CAS 登录、live smoke 写操作、C ABI、NAPI、HarmonyOS 和 Slint 相关内容属于 v0.5+ 后续阶段或实验路径；除非对应路线图阶段完成并更新版本号，否则不代表当前稳定承诺。

## 后续阶段

路线图中的后续阶段包括：

- **v0.4 — CLI 工程化**：已完成命令树稳定化、统一 JSON 输出、固定 exit code、配置/缓存命令、CLI 集成测试。
- **v0.5 — 真实 HTTP 与认证**：真实 HTTP 客户端、CAS 登录、Token 刷新、安全存储和 Cookie 持久化。
- **v0.6 — HarmonyOS NAPI**：C API 边界、NAPI 绑定、ArkTS 类型和错误映射。
- **v0.7 — HarmonyOS ArkUI**：登录页、课表、考试、教室和设置页。
- **v0.8 — Windows Slint GUI**：Slint UI、ViewModel、归属声明和 Core 复用。
- **v1.0 — 稳定版**：测试、CI/CD、发布自动化和平台打包完善。

## 构建要求

- CMake 3.21+
- Ninja 或 Visual Studio 生成器
- vcpkg manifest 依赖：`nlohmann-json`、`catch2`、`openssl`、`curl`
- Windows：MSVC 编译器，推荐配合 vcpkg toolchain

项目默认使用 C++17，可通过 `UBAANEXT_CXX_STANDARD=20` 验证 C++20 构建。

## Windows 构建与测试

```powershell
cmake --fresh --preset windows-ninja-msvc-debug
cmake --build --preset windows-ninja-msvc-debug --target ubaa
ctest --preset windows-ninja-msvc-debug
```

Release CLI：

```powershell
cmake --fresh --preset windows-ninja-msvc-release
cmake --build --preset windows-ninja-msvc-release --target ubaa
```

Release 可执行文件会输出到固定目录，便于直接查找：

```text
bin\x64\Release\ubaa.exe
bin\x64\Release\ubaa.exe.sha256
```

当前 `v0.4.0` 只要求完成 Release 编译验证和本地二进制输出，不创建 GitHub Release 页面。

如果本地已配置好构建目录，重复编译可直接运行：

```powershell
cmake --build --preset windows-ninja-msvc-debug --target ubaa
```

## CLI 示例

以下示例以 v0.4 基线能力为主，优先使用 mock/offline 路径：

```powershell
.\bin\x64\Debug\ubaa.exe version --json
.\bin\x64\Debug\ubaa.exe login --mock --username 20260000 --password test --json
.\bin\x64\Debug\ubaa.exe whoami --json
.\bin\x64\Debug\ubaa.exe course today --mock --json
.\bin\x64\Debug\ubaa.exe course week --mock --week 8 --json
.\bin\x64\Debug\ubaa.exe exam list --mock --json
.\bin\x64\Debug\ubaa.exe classroom query --mock --campus 1 --date 2026-05-13 --json
.\bin\x64\Debug\ubaa.exe term list --mock --json
.\bin\x64\Debug\ubaa.exe week list --mock --json
```

真实 HTTP、CAS 登录、live smoke 写操作、C ABI、NAPI、HarmonyOS 和 Slint 相关内容属于后续阶段或实验路径。运行涉及真实账号、远端服务或写操作的命令前，请先阅读对应文档并显式 opt-in。

## 安全边界

- 不要在普通回归中使用真实账号、密码或线上写操作。
- 涉及远端写操作的命令必须显式确认，并受平台 capability 约束。
- 日志、错误、diagnostics 和测试输出不得泄露 username、password、cookie、token、ticket、session、captcha、authorization、敏感 URL query 或 raw HTML。
- secure store、cookie/session persistence、crypto 或 live login 能力不可用时，应 fail-closed，而不是静默降级成不安全实现。

## 文档入口

- [文档首页](docs/README.md)
- [文档索引](docs/00-index.md)
- [路线图](docs/02-roadmap.md)
- [Windows CLI](docs/apps/windows-cli.md)
- [版本号规范](docs/release/versioning.md)
- [测试策略](docs/testing/test-strategy.md)

## 许可证

MIT License。详见 [LICENSE](LICENSE) 与 [NOTICE](NOTICE)。
