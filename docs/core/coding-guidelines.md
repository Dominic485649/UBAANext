# 编码规范

## C++ 标准

- **基准版本**: 默认 C++17，可选 C++20
- **不使用 C++23/26 标准库特性作为必需依赖**
- 使用标准库类型: `std::string`, `std::vector`, `std::optional`, `std::unordered_map`

## 代码风格

- **花括号风格**: Allman（花括号单独一行）
- **缩进**: 4 个空格
- **列宽限制**: 100 个字符
- **命名规范**:
  - 类/结构体: `PascalCase`
  - 函数/方法: `snake_case`
  - 变量: `snake_case`
  - 常量: `snake_case`
  - 命名空间: `PascalCase`
  - 枚举: 类型使用 `PascalCase`，值使用 `PascalCase`

## 头文件

- 使用 `#pragma once` 作为头文件保护
- 包含顺序: 自身头文件、项目头文件、第三方库头文件、标准库头文件
- 公共头文件位于 `core/include/UBAANext/`
- 禁止在公共头文件中暴露 `namespace um = UBAANext;`

## 错误处理

- 对可能失败的操作使用 `Result<T>`
- 禁止在公共 API 中抛出异常
- 错误码来自 `ErrorCode` 枚举
- 代码中的错误信息使用英文，面向用户时使用中文

## 现代 CMake

- 基于目标: `target_include_directories`, `target_link_libraries`
- 禁止使用全局 `include_directories` 或 `link_directories`
- 使用生成器表达式区分构建/安装接口
