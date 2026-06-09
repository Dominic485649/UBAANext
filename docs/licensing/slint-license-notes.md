# Slint 许可说明

> Slint 只在构建 `UBAANEXT_BUILD_DESKTOP=ON` 的实验性桌面目标时需要；默认 CLI/Core 构建不引入 Slint。最终分发前必须以实际锁定版本和官方许可文本为准。

## 当前工程状态

- 桌面源码目录：`apps/desktop/`
- CMake target：`UBAANextDesktop`
- Slint 查找方式：`find_package(Slint CONFIG QUIET)`
- FetchContent fallback：`UBAANEXT_FETCH_DEPS=ON` 时拉取 Slint `v1.16.1`
- 默认构建：`UBAANEXT_BUILD_DESKTOP=OFF`，不构建 Slint UI

Core、Platform、CLI 和 C ABI 不应包含 Slint 头文件或链接 `Slint::Slint`。Slint 依赖只允许进入桌面 shell target。

## 许可模式

Slint 提供多种授权路径，常见选择包括 GPLv3、免费桌面/嵌入式许可和商业许可。不同版本的条款可能变化，发布前必须核对官方许可文本和项目实际使用的 Slint 版本。

| 模式 | 影响 |
| --- | --- |
| GPLv3 | 分发链接 Slint 的 GUI 二进制时，GUI 组合产物需要满足 GPLv3 条款；MIT Core 可作为兼容依赖保留自身许可。 |
| 免费桌面/嵌入式许可 | 可能要求在 UI 中保留 Slint 署名或满足其他展示/用途限制；需按官方条款执行。 |
| 商业许可 | 适合需要移除署名或规避 copyleft 约束的分发；是否需要取决于发布目标。 |

## 发布前检查

分发任何 `ubaa.exe` / `ubaa-gui` 桌面构建前必须完成：

1. 确认实际使用的 Slint 版本和许可模式。
2. 更新 `docs/licensing/third-party-notices.md` 和打包 NOTICE。
3. 如果许可要求 UI 署名，确认 GUI 中已有显著的 Slint 署名或 About 页面。
4. 如果采用 GPLv3 路径，确认 GUI 相关源码、构建脚本和许可文本满足分发要求。
5. 如果采用商业许可，记录许可证来源和允许的分发范围，但不要把许可证密钥写入仓库。

当前仓库尚未完成正式桌面分发合规闭环，文档和 release checklist 不应宣称 Slint GUI 可正式发布。
