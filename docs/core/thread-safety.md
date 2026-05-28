# 线程安全与并发设计 (Thread Safety Policy)

本篇文档详述 `UBAANext` 原生核心库在处理并发请求、状态访问及多线程桥接集成时的线程安全模型与同步规范。

## 1. 核心库（Core）的无锁设计哲学

为了追求极高的运行时性能和极简的跨平台编译体积，**`UBAANextCore` 的绝大多数底层数据结构与服务均被设计为“非线程安全（Non-Thread-Safe）”的**：

*   **`MemoryCacheStore` (内存缓存)**：其内部使用了一个 `mutable std::unordered_map` 进行数据存储，并在 `get()` 读取时采用**惰性清除（Lazy Eviction）**的方式剔除过期的 TTL 条目。由于该 map 没有任何内部锁（如 `std::mutex` 或 `std::shared_mutex`），在多线程并发执行 `set()` 与 `get()` 时会发生数据损坏（Data Corruption）或未定义行为。
*   **`AuthService` 与业务 `Service`**：属于无状态或弱状态的业务编排对象，其内部未包含线程同步互斥锁。

---

## 2. 线程同步重任的外部托管（Shell / Bridge 托管）

基于 Core 层“不设锁以提速”的原则，系统的并发保护任务被全部**上浮并托管到 C ABI 桥接层（Bindings）或平台 Shell 侧**：

```
+------------------------------------------------------+
|            上层宿主环境 (ArkTS/NAPI 异步线程池)       |
+------------------------------------------------------+
                           |
                           v (并发请求)
+------------------------------------------------------+
|             C ABI Bridge (ubaanext_c)                |
|    - with_context 自动持有锁 std::lock_guard          |
|    - 互斥锁定当前 Context 的生命周期                  |
+------------------------------------------------------+
                           |
                           v (串行化安全调用)
+------------------------------------------------------+
|                  UBAANextCore                        |
|       (无锁、高速、平台无关的 C++ 业务核心)          |
+------------------------------------------------------+
```

### 2.1 C ABI 层的串行化保护机制
在 `ubaanext_c` 中，每个 `UbaaNextContext` 都绑定了一个互斥量：
```cpp
struct UbaaNextContext {
    UBAANext::PlatformCapabilities capabilities = current_capabilities();
    UBAANext::ConnectionMode mode = UBAANext::ConnectionMode::WebVPN;
    RuntimeBucket direct;
    RuntimeBucket webvpn;
    std::mutex mutex;  // 关键互斥锁
};
```
所有的 ABI 接口实现均在 `with_context` 模板保护下运行：
```cpp
template <typename Callback>
[[nodiscard]] const char *with_context(UbaaNextContext *context, Callback callback) {
    if (!context) return null_context_result();
    try {
        std::lock_guard<std::mutex> lock(context->mutex);  // 自动加锁与解锁
        return callback(*context);
    } catch (...) {
        // ...
    }
}
```
*   **安全屏障**：通过锁住 Context 内的 `mutex`，任何外部 NAPI 线程池（如 ArkTS 端的异步 Promise 并发调用）发起的并发请求，都将在 ABI 边界被强行**串行化（Serialized）**。这确保了在底层的 `MemoryCacheStore` 读写和 `CurlNetworkStack` 请求时，绝对不会发生多线程竞态条件。

---

## 3. 开发接入规范与死锁防范

当下游客户端或开发人员在 Native 层开发新的功能或对接时，必须遵守以下多线程防护守则：

1.  **切勿绕过 Context 加锁**：在 C++ 侧，若要直接调用核心库的业务 Service，必须确保每次调用都在特定的同步保护下进行，严禁让多个线程共享同一个未经 `mutex` 保护的 `ICacheStore` 或 `AuthService` 实例。
2.  **避免在异步回调中持有锁**：由于底层的 `IHttpClient` 网络调用是同步阻塞（Blocking）式的（通过 libcurl），`std::lock_guard` 会在整个网络传输期间一直持有 Context 锁。NAPI 侧在发起异步 Task 时，应当在独立的工作线程（Worker Thread）中调用 C ABI，并在 C ABI 返回结果后立即释放，**严禁在持有 Context 锁的过程中同步等待其他 UI 线程的事件，以防发生系统级死锁**。
3.  **模式切换时的安全防范**：在切换连接模式时（`ubaanext_context_set_connection_mode`），该函数同样会抢占 Context 锁，切换过程会安全挂起当前正在执行的业务请求，直到切换完成。
