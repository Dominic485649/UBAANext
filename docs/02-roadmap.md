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

## v0.4 — CLI 工程化

- 命令树稳定化，拆分 AppContext / ServiceFactory / CommandHandlers
- 所有命令支持 `--json` 输出
- 固定 exit code（0-6）
- config / cache clear 子命令
- CLI integration tests 与 golden output tests
- 文档完善（JSON 输出格式、错误码、CLI 命令 API）

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
