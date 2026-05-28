# 绑定设计 (Bindings Design)

本篇文档详述 `UBAANext` 原生核心库对外提供的稳定 C ABI 接口设计，以及为 HarmonyOS NAPI 集成所制定的核心合同规范。

## 1. 设计目标与边界原则

为了让跨平台的 C++ Core 服务（`UBAANextCore`）能够无缝、安全地注入到 OpenHarmony (DevEco/ArkTS) 工程及其他宿主客户端中，我们遵循了以下边界原则：

1. **二进制兼容性 (Stable ABI)**：使用纯 C 函数边界和 `extern "C"` 声明，配合平台专属导出宏（Windows 下的 `__declspec` 与 GCC/Clang 的 `visibility("default")`），保证动态库二进制导出级别兼容，不受 C++ 编译器 ABI 差异的影响。
2. **强类型在 Core，动态分发在 Bridge**：Core 内部使用 C++ 强类型结构体进行网络解析与业务建模；但在 C ABI 边界上，输入统一采用标准的 C 字符串或整型，输出统一序列化为 **JSON 格式字符串**，由调用端（如 NAPI 层或 ArkTS 侧）动态解析，极大降低了复杂的指针传递和内存对齐开销。
3. **内存生命周期严格归口**：规范 C++ 堆内存的分配与释放，任何由 Core 分配给 C ABI 调用方的字符串指针，必须通过指定的 ABI 函数进行释放，严防跨语言堆栈下的内存泄露。
4. **安全与敏感数据脱敏**：任何由于异常、解析失败或网络断开导致的错误消息，在越过 C ABI 边界时，必须强制经过 Core 的脱敏服务处理，杜绝用户敏感词泄漏到 JS/UI 层及设备日志中。

---

## 2. Context 生命周期与隔离设计

### 2.1 Context 的生命周期

客户端的会话、Cookie 和缓存均绑定在唯一的 `UbaaNextContext` 上。C ABI 提供了以下三个基础函数进行管理：

*   `ubaanext_context_create()`：在 C++ 动态堆中分配并构造一个新的 `UbaaNextContext` 实例。初始化时，该函数会隐式安装 OpenSSL 的 Crypto Provider，以确保 WebVPN 等模块所需的加密引擎就绪。如果分配失败，安全返回 `nullptr`。
*   `ubaanext_context_release(context)`：销毁 Context 实例，释放其内部持有的所有服务、网络栈和缓存，防止内存泄漏。
*   `ubaanext_context_set_connection_mode(context, mode)`：切换当前 Context 的连接模式。支持的值包括 `"direct"`（直连学校内网）、`"vpn"`/`"webvpn"`（通过北航 WebVPN 代理）以及在调试状态下启用的 `"mock"`（离线桩模式）。

### 2.2 线程安全与互斥锁
在 `UbaaNextContext` 中，内部嵌有 `std::mutex mutex`：
```cpp
struct UbaaNextContext {
    std::mutex mutex;
    // ...
};
```
在 C ABI 的核心包装函数中，所有的业务调用均会通过 `with_context` 辅助函数自动持有此互斥锁（`std::lock_guard`），从而实现多线程调用时的并发序列化。上层应用（如 NAPI 侧的异步 Promise 线程池）并发发起请求时，核心库将在 Context 级别安全地串行化执行。

### 2.3 连接模式分桶隔离 (Runtime Bucket)
Context 内部对三种网络模式持有了独立的运行时桶（`RuntimeBucket`）：
```cpp
struct RuntimeBucket {
    VolatileSecureStore store;
    UBAANext::MemoryCacheStore cache;
    UBAANext::Platform::Curl::CurlNetworkStack network{store};
};
```
*   **设计动机**：当用户在“直连”和“WebVPN”模式之间切换时，各网络请求所携带的 Cookie 域和 Session 凭据截然不同。
*   **物理隔离**：`RuntimeBucket` 为每种模式分配了完全**独立**的网络栈（`CurlNetworkStack`）、临时 Cookie 罐、缓存区（`MemoryCacheStore`）和临时凭据存储。这在底层物理上杜绝了因网络模式快速切换导致的 Cookie 串用或 WebVPN 票据泄漏的风险。

---

## 3. 内存分配与释放协议

C ABI 层为了向跨语言边界传递结构化数据，所有查询 API 统一使用 `const char *` 返回 JSON 格式的数据。为避免内存越界和多分配器（Multi-Allocator）冲突，定义了严格的内存协议：

*   **数据拷贝**：C ABI 在底层使用 `new[]` 手动分配空间，并将 Core 生成的序列化 JSON 字符串完整复制进去（见 `copy_result` 函数）。
*   **释放责任**：调用端（如 NAPI 侧的 C++ 包装层）在读取并将其转换为 NAPI JS 类型的对象后，**必须立即调用 `ubaanext_release_result` 释放该指针**。
*   **安全性**：如果在 Context 销毁前遗漏了释放，或者尝试跨 Allocator 使用标准的 `free` 或 `delete` 释放该指针，都会导致严重的内存泄漏或运行时崩溃。

---

## 4. 业务数据 JSON Envelope 设计

C ABI 对外输出的 JSON 字符串统一采用包装盒（Envelope）模式，以确保客户端在处理错误与数据时拥有统一的解析范式。

### 4.1 成功数据外壳 (Success Envelope)
当业务执行成功时，外壳将 `ok` 设为 `true`，`error` 设为 `null`，真实业务数据承载于 `data` 节点：
```json
{
  "ok": true,
  "data": {
    "courses": [
      {
        "id": "1001",
        "name": "软件工程",
        "teacher": "张教授",
        "classroom": "学院路一号楼301",
        "weekStart": 1,
        "weekEnd": 16,
        "dayOfWeek": 1,
        "sectionStart": 1,
        "sectionEnd": 2,
        "courseCode": "SE2026",
        "credit": 3.0,
        "beginTime": "08:00",
        "endTime": "09:50"
      }
    ]
  },
  "error": null
}
```

### 4.2 失败/错误外壳 (Failure Envelope)
当调用失败（包括参数无效、未认证、网络崩溃、平台不支持等）时，`ok` 设为 `false`，`data` 设为 `null`，并在 `error` 节点中返回标准的错误码与脱敏后的诊断消息：
```json
{
  "ok": false,
  "data": null,
  "error": {
    "code": "NetworkError",
    "message": "网络连接超时，请检查您的校园网环境"
  }
}
```

---

## 5. Volatile 缓存与存储保护

由于部分平台（例如当前阶段的 OpenHarmony 平台适配层）尚未真正落地持久化的安全凭据存储（即 `HarmonyPlatformCapabilities` 声明 `secure_store = false`），底层在尝试调用平台原生安全存储写入凭据时会通过 `UnsupportedSecureStore` 强行抛出异常，防止以明文形式向不可信磁盘回退。

为了规避这种在 Harmony 桥接早期可能导致的崩溃问题，C ABI 采取了**运行时隔离兜底设计**：
*   在 `UbaaNextContext` 的隔离运行时桶内，我们强制实例化了在内存中运行的 `VolatileSecureStore`。
*   这使得 Context 在整个生命周期内能够正常维持登录状态、Cookie 存储与 WebVPN 密钥加解密，**彻底规避了因 UnsupportedSecureStore 写调用导致的运行时 Crash**。
*   **副作用提示**：由于此存储属于纯内存性质，一旦客户端 Context 销毁（如 App 进程被杀或被冷启动），所有的登录会话凭据将被立即清除，下一次启动时用户必须重新执行真实登录。这也是 HarmonyOS 后续接入持久化方案时需重点攻克的方向。

---

## 6. 已导出的 22 个符号清单与业务映射

| 符号名称 | 对应核心服务 / 作用 | 输入参数说明 | 输出 JSON 节点含义 |
| :--- | :--- | :--- | :--- |
| **`ubaanext_version`** | 获取原生 SDK 内部硬编码的版本号（无 IO 损耗） | `void` | 返回 `"0.4"` 等硬编码版本字符串 |
| **`ubaanext_get_capabilities`** | 获取当前操作系统物理适配层的能力清单 | `UbaaNextCapabilities*` 接收指针 | 写入真实的网络、存储、写门控等状态布尔值 |
| **`ubaanext_context_create`** | 堆中构建 Context 实例，加载加密 Provider | `void` | 返回 `UbaaNextContext` 结构体指针 |
| **`ubaanext_context_release`** | 销毁 Context 实例，彻底释放内存 | `UbaaNextContext*` | `void` |
| **`ubaanext_context_set_connection_mode`** | 更改连接模式 (`direct`/`webvpn`/`mock`) | `context`, `mode` 字符串 | 返回整型退出码（`0` 成功，负数失败） |
| **`ubaanext_release_result`** | **【核心】** 释放 C ABI 层分配的 JSON 字符串内存 | `const char*` 待释放指针 | `void` |
| **`ubaanext_auth_login`** | 调用 `AuthService` 执行真实 SSO/WebVPN 认证登录 | `context`, `username`, `password`, `captcha` | 返回 `{"account": {"studentId": "...", "displayName": "..."}}` |
| **`ubaanext_auth_logout`** | 调用 `AuthService` 注销登录并清除底层 volatile 会话 | `context` | 返回 `{"active": false}` |
| **`ubaanext_auth_restore_session`** | 从 volatile store 中尝试恢复现有的会话凭据 | `context` | 返回 `{"active": true, "account": {...}}` |
| **`ubaanext_auth_get_session_state`** | 实时获取当前的登录活跃状态与当前所处连接模式 | `context` | 返回当前活跃度 `active` 与连接模式 `mode` |
| **`ubaanext_terms`** | 调用 `TermService` 获取学期列表（含当前选中 index） | `context` | 返回 `{"terms": [...]}` |
| **`ubaanext_weeks`** | 调用 `TermService` 查询指定学期的教学周排表 | `context`, `term_code` | 返回 `{"weeks": [...]}` |
| **`ubaanext_courses_today`** | 调用 `CourseService` 获取系统今日的课表 | `context` | 返回 `{"courses": [...]}` |
| **`ubaanext_courses_date`** | 获取指定日期（格式 `yyyy-MM-dd`）的课表数据 | `context`, `date` 字符串 | 返回 `{"courses": [...]}` |
| **`ubaanext_courses_week`** | 获取指定教学周、指定学期（可空）的完整课表 | `context`, `week` 整数, `term_code` | 返回 `{"courses": [...]}` |
| **`ubaanext_grades`** | 调用 `GradeService` 查询指定学期（为空则为全量）成绩 | `context`, `term_code` | 返回 `{"grades": [...]}`（数据受安全脱敏保护） |
| **`ubaanext_exams`** | 调用 `ExamService` 查询指定学期的考试日程安排 | `context`, `term_code` | 返回 `{"exams": [...]}` |
| **`ubaanext_todos`** | 调用 `TodoService` 获取待办事项聚合列表 | `context`, `pending_only` 布尔值 | 返回 `{"todos": [...]}`（支持携带局部失败记录） |
| **`ubaanext_signin_today`** | 调用 `SigninService` 获取今日的可签到课程列表 | `context` | 返回 `{"courses": [...]}` |
| **`ubaanext_signin_do`** | 执行签到操作（**高风险写操作，受门控限制**） | `context`, `course_id`, `confirmed` 布尔确认 | 返回 `{"mutation": {"accepted": bool, "message": "..."}}` |
| **`ubaanext_ygdk_overview`** | 调用 `YgdkService` 获取阳光打卡的本学期汇总概览数据 | `context` | 返回打卡概览 `overview` 与支持的分类 `items` |
| **`ubaanext_ygdk_records`** | 分页查询阳光打卡的打卡历史记录 | `context`, `page`, `size` 整数 | 返回打卡记录数组 `{"records": [...]}` |
