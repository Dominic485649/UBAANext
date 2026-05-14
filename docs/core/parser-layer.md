# 解析器层

## 概述

Parser 层负责将 UBAA API 返回的 JSON 响应字符串解析为 C++ Model 结构体。
位于 `core/include/UBAANext/Parser/JsonParser.hpp` 和 `core/src/Parser/JsonParser.cpp`。

## JSON 库选型

v0.3 引入 [nlohmann/json](https://github.com/nlohmann/json)（MIT 许可）：

- **Header-only**：无需额外链接，通过 vcpkg 集成
- **C++ 生态最广泛使用的 JSON 库**：文档完善、社区活跃
- **API 简洁**：`json::parse()` + `value()` 即可完成大部分解析
- **异常安全**：JSON 语法错误抛出 `json::exception`，统一捕获为 `ParseError`

## 解析函数

| 函数 | 输入 | 输出 | 说明 |
|------|------|------|------|
| `parse_courses` | JSON 数组 | `Result<vector<Course>>` | 课程列表 |
| `parse_exams` | JSON 数组 | `Result<vector<Exam>>` | 考试列表 |
| `parse_classrooms` | JSON 对象 | `Result<ClassroomQueryResult>` | 教室可用性 |
| `parse_terms` | JSON 数组 | `Result<vector<Term>>` | 学期列表 |
| `parse_weeks` | JSON 数组 | `Result<vector<Week>>` | 教学周列表 |

## 错误处理约定

| 场景 | 处理方式 |
|------|----------|
| JSON 语法错误 | 返回 `ParseError` + nlohmann 错误消息 |
| 类型不匹配（期望数组得到对象） | 返回 `ParseError` |
| 缺失字段 | 使用默认值，不视为错误（容错解析） |
| 必填字段（如 id）为空 | 不在此层校验，由 Service 层处理 |

## 数据映射规则

JSON 键名使用 camelCase（与 JavaScript/TypeScript 惯例一致），
Model 字段使用 snake_case（C++ 惯例）。映射在解析函数中硬编码：

```cpp
c.week_start = item.value("weekStart", 0);
c.day_of_week = item.value("dayOfWeek", 0);
```

## 设计决策

- **不在 Model 中添加 JSON 注解**：保持 Model 为纯数据结构，不依赖 JSON 库
- **不在 Parser 中做业务校验**：Parser 只负责格式转换，数据合法性由 Service 判断
- **容错优先**：缺失字段使用默认值，不因单个字段缺失而整体失败

