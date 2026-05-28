# HarmonyOS 授权与 OpenHarmony SDK 依赖许可规范

本文档详尽规范并说明了 UBAA Next 项目在适配 HarmonyOS（鸿蒙系统）以及与 OpenHarmony SDK 结合使用时的许可证合规性、底层依赖链许可要求及代码分发边界。

---

## 1. 跨平台架构与双项目法律边界

为了确保 UBAA 核心业务逻辑与各平台界面展现层的法律安全以及独立性，项目采取了严格的**“双项目分工”**设计。其知识产权与许可结构如下：

### 1.1 C++ 原生核心仓库 (`D:\Code\Cpp\UBAANext`)
* **许可模式**：全面采用宽松的 **MIT 许可证** 授权。
* **业务范畴**：承载校园服务业务模型、底层网络协议解析、统一认证及会话管理（Authentication & Session）、网络抽象层、安全存储抽象、数据脱敏（Redaction）以及写入网关（Write Gate）逻辑。
* **交付物**：面向鸿蒙平台交付名为 `libubaanext_c.so` 的共享库（由 `UBAANextBindingsC` 编译目标产出，符合标准的 C ABI 规范）以及一组经过严格设计的公共头文件。

### 1.2 鸿蒙应用外壳仓库 (`D:\Code\OpenHarmony\UBAANext`)
* **许可模式**：由鸿蒙外壳团队根据具体的发布渠道另行指定（例如商业闭源或宽松开源）。
* **业务范畴**：仅包含 DevEco Stage 模型下的 HAP/HSP 工程、ArkTS 业务逻辑、ArkUI 声明式界面、系统 Ability 生命周期管理、HAP 包签名工具及设备端调试配置。
* **重要红线**：**鸿蒙外壳项目内严禁复制或重新实现**本项目的校园系统核心协议解析、数据脱敏、加密算法、Cookie/Session 还原状态等底层核心逻辑。鸿蒙外壳仅通过 NAPI 桥接层，以受控方式调用 C++ 核心库，绝对避免底层敏感逻辑“二次膨胀”及版权碎片化。

---

## 2. OpenHarmony SDK 与系统依赖合规

在构建面向 OpenHarmony 的共享二进制文件时，必须集成 OpenHarmony Native SDK。该 SDK 的主体及其周边工具链遵循如下开源许可：

### 2.1 Native SDK 许可证兼容性
* **编译器与工具链**：OpenHarmony Native SDK 中采用的 Clang/LLVM 编译器、CMake 构建工具等，均由官方基于宽松开源协议（如 Apache 2.0 with LLVM Exception、BSD 等）分发。这与 UBAA Next 的 MIT 许可完全兼容。
* **NAPI 桥接标准**：项目采用的 NAPI（Node-API）规范是 OpenHarmony 的标准原生交互 API，其声明头文件及符号链接库同样采用宽松许可（Apache 2.0），不包含任何传染性开源条款（如 GPL/LGPL），因此第三方闭源或宽松开源的 HAP 应用链接该接口不存在合规风险。

### 2.2 目标系统 ABI 约束
针对鸿蒙系统打包发布时，编译链目标 ABI 统一锁定为 **`arm64-v8a`**。所生成的 Native `.so` 库在动态链接鸿蒙系统基础 C 库（如 `libc.so`、`libhilog.ndk.z.so` 等）时，属于标准的系统 API 调用，受系统商业授权保护，不存在额外开源合规限制。

---

## 3. 第三方依赖包的许可要求

UBAA Next 在为鸿蒙平台提供底层服务支持时，必须静态或动态地引入部分不可或缺的第三方库。其许可证性质及我们在鸿蒙打包时的合规操作如下表所示：

| 依赖库名称 | 用途说明 | 开源许可证 | 鸿蒙集成方式与合规红线 |
| :--- | :--- | :--- | :--- |
| **nlohmann-json** | 核心配置与 API 响应的 JSON 解析 | **MIT License** | 属于 Header-only 库。在鸿蒙 Preset 构建中，直接通过外部指定的 CMake 包含路径解析。在发布包中需在 NOTICE 归属文件中保留原作者版权声明。 |
| **libcurl** | 提供底层的真实网络栈与 HTTP 协议传输 | **curl License** | 必须由鸿蒙编译链明确引入对应 `arm64-v8a` ABI 的二进制版本。分发时必须在 HAP 随包携带的第三方声明中保留 curl 的版权声明及许可文本。 |
| **OpenSSL** | 提供数据传输加密及会话签名等安全机制 | **Apache License 2.0** | 与 libcurl 相同，必须采用与鸿蒙平台兼容的编译版本。其许可证要求分发时必须保留原作者的版权信息、Apache 2.0 完整文本以及必要的 Notice 声明。 |
| **Catch2** | 单元测试与离线模拟回归测试框架 | **BSL-1.0** | **只用于主机端（Host）本地测试**。在为鸿蒙系统构建发布版本（HAP 包）时，编译 preset 会自动关闭测试目标并彻底剥离 Catch2，避免将测试代码带入最终的包体中。 |

---

## 4. 鸿蒙发布包（HAP）合规审计清单

在 DevEco Studio 中执行 HAP 打包并面向用户分发时，必须确保完全履行以下合规及安全合规义务：

1. **附带第三方版权声明（Third-Party Notices）**：
   在最终交付的 HAP 资源包中（例如 `entry/src/main/resources/rawfile/` 目录下），必须随包发布统一的第三方版权声明文件，列出上述所有被静态或动态链接的第三方组件的原始许可证文本（含 `nlohmann-json`、`curl`、`OpenSSL`）。
2. **严防敏感资产与凭据遗留**：
   鸿蒙外壳工程打包时，必须通过 `keepARGs` 或相关的打包排除规则，确保 HAP 包内部**绝对不包含**：
   * 开发调试阶段的测试凭据、Mock 离线数据文件、本地密钥文件。
   * Host 端 CLI 二进制程序（如 Windows/Linux 版本的 `ubaa.exe` 或 `ubaa`）。
   * 任何包含敏感开发路径、测试脚本（如 `live-smoke.ps1`）等调试附属物。
3. **敏感数据脱敏边界（Redaction Compliance）**：
   由 `UBAANextCore` 执行的所有数据脱敏（Redaction）操作应在 C++ 核心层内闭环完成。跨越 NAPI C ABI 边界传递到 ArkTS 层的数据，必须已经是被脱敏后的内容。严防未经脱敏的用户名、密码、原始 Cookie、会话 Token、上传文件路径及成绩、打卡等隐私信息暴露在 ArkTS 侧的系统日志（HiLog）或终端控制台中。

---

UBAA Next 鸿蒙适配团队应严格遵守本规范。如在后续迭代中引入全新的三方依赖，必须第一时间将新依赖的许可文本追加入此文档并更新 NOTICE。
