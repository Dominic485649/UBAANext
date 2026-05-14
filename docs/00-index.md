# 文档索引

## 概览

- [项目概览](01-project-overview.md)
- [路线图](02-roadmap.md)
- [术语表](03-glossary.md)

## 架构

- [整体架构](architecture/overall-architecture.md)
- [模块边界](architecture/module-boundaries.md)
- [依赖规则](architecture/dependency-rules.md)
- [核心设计](architecture/core-design.md)
- [适配器设计](architecture/adapter-design.md)
- [绑定设计](architecture/binding-design.md)
- [CLI 设计](architecture/cli-design.md)
- [Harmony ArkUI 设计](architecture/harmony-arkui-design.md)
- [Slint 桌面设计](architecture/slint-desktop-design.md)
- [未来扩展点](architecture/future-extension-points.md)

## 核心

- [编码规范](core/coding-guidelines.md)
- [命名规则](core/naming-rules.md)
- [命名空间规则](core/namespace-rules.md)
- [错误模型](core/error-model.md)
- [Result 类型](core/result-type.md)
- [模型层](core/model-layer.md)
- [服务层](core/service-layer.md)
- [解析器层](core/parser-layer.md)
- [网络抽象](core/net-abstraction.md)
- [存储抽象](core/storage-abstraction.md)
- [会话设计](core/session-design.md)
- [缓存设计](core/cache-design.md)
- [线程安全](core/thread-safety.md)

## API

- [公共 C++ API](api/public-cpp-api.md)
- [C API 计划](api/c-api-plan.md)
- [NAPI API 计划](api/napi-api-plan.md)
- [CLI 命令 API](api/cli-command-api.md)
- [错误码](api/error-codes.md)
- [JSON 输出格式](api/json-output-format.md)

## 构建

- [CMake 指南](build/cmake-guide.md)
- [CMake 预设](build/cmake-presets.md)
- [Windows MSVC 构建](build/windows-msvc-build.md)
- [Windows Clang 构建](build/windows-clang-build.md)
- [Linux 构建计划](build/linux-build-plan.md)
- [Harmony 原生构建计划](build/harmony-native-build-plan.md)
- [依赖策略](build/dependency-policy.md)
- [工具链说明](build/toolchain-notes.md)

## 测试

- [测试策略](testing/test-strategy.md)
- [单元测试](testing/unit-tests.md)
- [集成测试计划](testing/integration-tests-plan.md)
- [Mock 数据策略](testing/mock-data-policy.md)
- [基准文件](testing/golden-files.md)
- [CI 计划](testing/ci-plan.md)

## 应用

- [Windows CLI](apps/windows-cli.md)
- [Harmony ArkUI](apps/harmony-arkui.md)
- [Windows Slint GUI](apps/windows-slint-gui.md)
- [发布渠道计划](apps/release-channel-plan.md)

## 安全

- [凭证处理](security/credential-handling.md)
- [安全存储](security/secure-store.md)
- [Cookie 会话策略](security/cookie-session-policy.md)
- [隐私政策草案](security/privacy-policy-draft.md)
- [敏感数据策略](security/sensitive-data-policy.md)
- [威胁模型](security/threat-model.md)

## 许可

- [许可策略](licensing/license-strategy.md)
- [MIT 许可说明](licensing/mit-license-notes.md)
- [第三方声明](licensing/third-party-notices.md)
- [原 UBAA 声明](licensing/original-ubaa-notice.md)
- [Slint 许可说明](licensing/slint-license-notes.md)
- [Harmony 许可说明](licensing/harmony-license-notes.md)

## 协议

- [Mock 协议](protocol/mock-protocol.md)
- [认证流程计划](protocol/auth-flow-plan.md)
- [课程数据计划](protocol/course-data-plan.md)
- [考试数据计划](protocol/exam-data-plan.md)
- [教室数据计划](protocol/classroom-data-plan.md)
- [解析器变更策略](protocol/parser-change-policy.md)

## 发布

- [版本号规范](release/versioning.md)
- [变更日志策略](release/changelog-policy.md)
- [Windows 打包](release/packaging-windows.md)
- [Harmony 打包](release/packaging-harmony.md)
- [GitHub 发布检查清单](release/github-release-checklist.md)
- [分发计划](release/distribution-plan.md)

## 决策记录

- [0001: 使用 C++17/C++20 作为基线](decisions/0001-use-cpp23-as-baseline.md)
- [0002: 使用 UBAANext 命名空间](decisions/0002-use-ubaanext-namespace.md)
- [0003: 使用 C++ Core + Platform Shell](decisions/0003-use-cpp-core-plus-platform-shell.md)
- [0004: 使用 ArkUI 用于 HarmonyOS](decisions/0004-use-arkui-for-harmonyos.md)
- [0005: Windows 上优先使用 CLI](decisions/0005-use-cli-first-on-windows.md)
- [0006: 保留 Slint 作为未来 Windows GUI](decisions/0006-keep-slint-as-future-windows-gui.md)
