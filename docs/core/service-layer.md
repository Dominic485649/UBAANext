# 服务层

## 概述

Service 层是业务逻辑的核心，位于 `core/include/UBAANext/Service/` 和 `core/src/Service/`。
每个 Service 类通过依赖注入接收 `IHttpClient` 和 `ICacheStore` 接口。

## 已实现的 Service

### AuthService

认证与会话管理服务。

- **文件**：`Auth/AuthService.hpp`、`Auth/AuthService.cpp`
- **依赖**：`IHttpClient`、`ISecureStore`
- **接口**：
  - `login_mock(username, password)` → `Result<Account>`
  - `logout()` → `Result<void>`
  - `restore_session()` → `Result<Account>`
  - `has_session()` → `bool`

### CourseService

课程表查询服务。

- **文件**：`Service/CourseService.hpp`、`Service/CourseService.cpp`
- **依赖**：`IHttpClient`、`ICacheStore`
- **接口**：
  - `get_today_courses()` → `Result<vector<Course>>`
  - `get_week_courses(week)` → `Result<vector<Course>>`
- **数据流**：Mock HTTP → JsonParser → Cache → Return

### ExamService

考试日程查询服务。

- **文件**：`Service/ExamService.hpp`、`Service/ExamService.cpp`
- **依赖**：`IHttpClient`、`ICacheStore`
- **接口**：
  - `get_exams()` → `Result<vector<Exam>>`
- **数据流**：Mock HTTP → JsonParser → Cache → Return

### ClassroomService

教室可用性查询服务。

- **文件**：`Service/ClassroomService.hpp`、`Service/ClassroomService.cpp`
- **依赖**：`IHttpClient`、`ICacheStore`
- **接口**：
  - `query_classrooms(campus_id, date)` → `Result<ClassroomQueryResult>`
- **数据流**：Mock HTTP → JsonParser → Cache → Return

### TermService

学期/周次查询服务（v0.3 新增）。

- **文件**：`Service/TermService.hpp`、`Service/TermService.cpp`
- **依赖**：`IHttpClient`、`ICacheStore`
- **接口**：
  - `get_terms()` → `Result<vector<Term>>`
  - `get_weeks(term_code)` → `Result<vector<Week>>`
- **数据流**：Mock HTTP → JsonParser → Cache → Return

## 设计原则

1. **依赖倒置**：所有 Service 通过接口（`IHttpClient`、`ICacheStore`）访问外部资源
2. **构造函数注入**：依赖在构造时传入，不使用全局状态
3. **缓存透明**：缓存逻辑内聚在 Service 内部，调用方无需关心
4. **错误传播**：使用 `Result<T>` 统一错误处理，不抛异常

