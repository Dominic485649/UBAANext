# 路线图

## v0.1 — C++ Core 骨架 ✅

- Core 静态库（Result/Error、Model、Net/Storage 抽象）
- Mock 实现（MockHttpClient、MockSecureStore）
- AuthService（mock 登录/登出）
- CourseService（mock 今日课程）
- Windows CLI 最小入口（version、login、course today）
- 单元测试（Result、AuthService、CourseService）
- 完整文档结构

## v0.2 — 登录与会话 ✅

- AuthService 正式化
- SessionManager
- CookieJar
- SecureStore 适配器
- Windows DPAPI 适配器（CLI 使用 PlainFileStore 开发态存储）
- CLI: version/login/logout/whoami/course/exam/classroom

## v0.3 — 数据解析与缓存 ✅

- JSON 解析库（nlohmann/json，通过 vcpkg）
- JsonParser: parse_courses、parse_exams、parse_classrooms、parse_terms、parse_weeks
- MockHttpClient 基于 URL 路由的 JSON 响应
- Service 层: Mock HTTP → Parser → Cache → Return
- CourseService、ExamService、ClassroomService、TermService
- MemoryCacheStore 支持 TTL
- CLI: term list、week list 命令
- 全面单元测试（JsonParser、TermService、缓存集成）

## v0.4 — CLI 工程化 ✅

- 命令树稳定化：CLI 命令目录、文本 help、`help --json` 合同与命令识别集中到 `CommandHandlers`
- 运行时边界拆分：`AppContext` 承载进程级依赖，`ServiceFactory` 集中创建 Core Service，命令处理逻辑与平台资源装配解耦
- 所有公开 CLI 命令支持统一 `--json` envelope：`ok/data/error` 成功与失败格式固定
- 固定 CLI exit code（0-6）：成功、通用失败、参数错误、认证缺失、网络错误、解析错误、存储错误可被脚本稳定消费
- `config show`、`config set --confirm`、`cache clear --confirm` 子命令纳入稳定命令合同
- 写操作和本地破坏性操作继续 fail-closed：缺少 `--confirm` / `--yes` 或平台 `write_operations` capability 时不得触发远端写请求
- CLI integration tests 与 golden output tests 覆盖 help 命令目录、JSON envelope、exit code 范围、config/cache 确认门和关键 mock/offline 命令
- 文档同步 CLI 命令 API、JSON 输出格式、错误码、Windows CLI、测试策略和版本号说明
- 为 v0.5 真实测试准备安全边界：默认测试离线，live smoke 必须显式设置 `UBAANEXT_LIVE=1`，真实凭据不得入库、不得进入日志或测试快照，L1 只读失败不得被已知失败机制掩盖

## v0.5 — 真实 HTTP 与认证

- 实现真实 HTTP 客户端（libcurl 或 WinHTTP）
- CAS 登录流程（execution token、验证码处理）
- Token 刷新逻辑
- Windows DPAPI 安全存储
- 跨会话 Cookie 持久化
- HTTPS 证书验证

## v0.6 — HarmonyOS NAPI

- C API 边界
- NAPI 绑定
- ArkTS .d.ts 类型定义
- 异步 Promise 封装
- Error 映射

## v0.7 — HarmonyOS ArkUI

- 登录页
- 今日课表
- 考试列表
- 教室查询
- 设置页

## v0.8 — Windows Slint GUI

- Slint UI 定义
- C++ ViewModel
- AboutSlint / 归属声明
- Core 复用

## v1.0 — 稳定版

- 文档完善
- 测试覆盖完善
- CI/CD 流水线
- 发布自动化
- Windows 打包
- HarmonyOS 打包
