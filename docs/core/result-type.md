# Result 类型

## 概述

`Result<T>` 是 UBAA Next 中主要的错误处理类型。它要么持有一个值（`Ok`），要么持有一个错误（`Fail`）。

## API

### `Result<T>`

```cpp
// 创建 Ok 结果
auto result = Result<int>::Ok(42);

// 创建 Fail 结果
auto result = Result<int>::Fail(ErrorCode::Unknown, "something went wrong");

// 检查
if (result.has_value()) { ... }
if (result) { ... }  // explicit operator bool

// 访问
const int& val = result.value();
const Error& err = result.error();
```

### `Result<void>`

```cpp
// 创建 Ok 结果
auto result = Result<void>::Ok();

// 创建 Fail 结果
auto result = Result<void>::Fail(ErrorCode::InvalidArgument, "empty input");

// 检查
if (result.has_value()) { ... }
```

## 设计决策

- 不抛出异常 — 显式错误传播
- 值访问使用移动语义
- `explicit operator bool` 防止隐式转换陷阱
