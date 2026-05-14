# 核心设计

## Core 内部结构

```
core/
├── include/UBAANext/
│   ├── Base/          # Error, Result, Types
│   ├── Model/         # Account, Course, Exam
│   ├── Net/           # IHttpClient, HttpRequest, HttpResponse
│   ├── Storage/       # ISecureStore, ICacheStore
│   ├── Auth/          # AuthService, Session
│   └── Service/       # CourseService
└── src/
    ├── UBAANext.cpp
    ├── Auth/          # AuthService 实现
    └── Service/       # CourseService 实现
```

## 各层职责

### 基础层
- `Error` — 错误码 + 消息
- `Result<T>` / `Result<void>` — Ok/Fail 包装器
- `Types` — 通用类型别名

### 模型层
- 纯数据结构（struct）
- 无行为，无依赖
- 平台无关

### 抽象层
- `IHttpClient` — HTTP 请求/响应
- `ISecureStore` — 安全键值存储
- `ICacheStore` — 缓存存储

### 服务层
- `AuthService` — 登录/登出/会话管理
- `CourseService` — 课程数据获取
- 依赖抽象（IHttpClient、ISecureStore）
