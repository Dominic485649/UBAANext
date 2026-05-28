# Harmony 打包

## 双项目分工

当前项目 `D:\Code\Cpp\UBAANext` 是跨平台 native 真源，负责 Core、service、parser、protocol、platform adapter、CLI、测试、redaction、write gate 和 native SDK/package 输出。

鸿蒙项目 `D:\Code\OpenHarmony\UBAANext` 只负责 DevEco Stage/HAP 工程、ArkTS/ArkUI、Ability 生命周期、HAP 打包、签名和设备调试。它不得复制当前项目的校园系统协议、parser、service、crypto、cookie/session 或 redaction 逻辑。

## 推荐复用方式

长期复用方式是当前项目输出 native SDK/package，DevEco 项目通过 CMake package、exported target 或受控源码子构建消费：

1. 当前项目构建并安装 `UBAANextCore`、platform targets、可选 `UBAANextBindingsC` 和 public headers。
2. 当前项目生成 `UBAANextConfig.cmake`、`UBAANextConfigVersion.cmake` 和 `UBAANextTargets.cmake`。
3. DevEco `entry/src/main/cpp/CMakeLists.txt` 通过 `find_package(UBAANext CONFIG REQUIRED)` 或受控路径变量引用当前项目。
4. DevEco HAP 只打包必要 native `.so`、NAPI wrapper、manifest 和资源，不打包 CLI exe、tests、fixtures、凭据或本地敏感配置。

手工 `.so` 只允许作为早期 smoke 或 CI artifact 缓存，不作为长期主方案；若使用，必须携带 version、commit sha、OHOS ABI、编译器、依赖版本和 capability manifest。

## 当前 native 输出

本项目已提供以下迁移准备项：

- `UBAANEXT_BUILD_CLI`：host/Windows 默认开启，OpenHarmony/DevEco preset 关闭。
- `UBAANEXT_BUILD_BINDINGS`：默认关闭，OpenHarmony preset 开启。
- `UBAANextBindingsC`：最小 C ABI shared target，输出 `ubaanext_c`，当前只暴露 version 和 capabilities。
- CMake install/export package 骨架：安装 public headers、targets 和 package config。

当前 Windows package smoke 已验证：外部消费项目可通过 `find_package(UBAANext CONFIG REQUIRED)` 链接 `UBAANext::UBAANextBindingsC`，但消费侧必须同时提供与 SDK 同 ABI、同 triplet、同依赖来源的 CURL/OpenSSL/nlohmann-json 前缀。若 SDK 由 vcpkg manifest mode 在构建目录内解析依赖，消费 smoke 也应把对应 `vcpkg_installed/<triplet>` 放入 `CMAKE_PREFIX_PATH`；仅依赖全局 vcpkg installed 目录可能因依赖未安装、triplet 不一致或 config target 不一致而失败。

## 依赖规则

- `nlohmann-json`：header-only，由当前项目 CMake 解析 include 来源。
- `CURL` / `OpenSSL`：必须按 OHOS ABI 明确来源；可来自 DevEco/OpenHarmony SDK 兼容预编译包、受控外部构建或 CI artifact，但必须进入 manifest。
- `Catch2`：只用于 host/native tests，不进入 HAP。
- 不因 HAP 迁移删除 curl、OpenSSL、nlohmann-json、Catch2。
- third-party notices 必须随 HAP/release packaging 同步。

## HAP 打包检查

DevEco 项目验证时必须确认：

- HAP 包含正确 ABI 目录下的 NAPI/native `.so`。
- HAP 不包含 CLI exe、测试二进制、fixtures、凭据、cookie/session dump、本地配置或开发机路径。
- `secure_store=false`、`cookie_persistence=false`、`live_login=false`、`write_operations=false` 等 capability 不被 UI 壳隐藏。
- NAPI smoke 只调用 version/capability/mock-offline，不调用真实登录、真实写或通用上传。
- redaction 结果能穿过 C ABI/NAPI，不泄露 username、password、cookie、token、ticket、session、captcha、authorization、URL query、本地路径、上传文件名、成绩、锁码、预约、打卡或座位敏感原文。

## 状态声明

`.so` 可加载、HAP 可构建、NAPI smoke 通过，都不代表原 UBAA 后端语义已对齐，也不代表真实登录、secure store、cookie persistence 或真实写操作完成。真实只读 UI 必须等待 NAPI/C API 合同和对应离线回归完成；真实登录 UI 与真实写 UI 继续作为后续独立阶段。
