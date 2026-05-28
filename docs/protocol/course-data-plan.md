# 课程数据计划

## 概述

课表数据协议定义了 UBAA 课表 API 的请求/响应格式。
当前 v0.4 仍使用 Mock HTTP 返回硬编码 JSON，真实 API 对接进入 v0.5+。

## API 端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/schedule/today` | GET | 获取今日课程 |
| `/schedule/week` | GET | 获取指定周次课程 |

## 响应格式

JSON 数组，每个元素代表一个已排课程：

```json
[
  {
    "id": "COURSE001",
    "name": "高等数学",
    "teacher": "张教授",
    "classroom": "J3-101",
    "weekStart": 1,
    "weekEnd": 16,
    "dayOfWeek": 1,
    "sectionStart": 1,
    "sectionEnd": 2,
    "courseCode": "MATH101",
    "credit": "3.0",
    "beginTime": "08:00",
    "endTime": "09:40"
  }
]
```

## 字段映射

| JSON 键 | Model 字段 | 类型 | 说明 |
|---------|-----------|------|------|
| `id` | `id` | string | 唯一课程实例 ID |
| `name` | `name` | string | 课程名称 |
| `teacher` | `teacher` | string | 授课教师 |
| `classroom` | `classroom` | string | 上课地点 |
| `weekStart` | `week_start` | int | 起始周次 |
| `weekEnd` | `week_end` | int | 结束周次 |
| `dayOfWeek` | `day_of_week` | int | 星期几（1=周一） |
| `sectionStart` | `section_start` | int | 起始节次 |
| `sectionEnd` | `section_end` | int | 结束节次 |
| `courseCode` | `course_code` | string | 官方课程代码 |
| `credit` | `credit` | string | 学分 |
| `beginTime` | `begin_time` | string | 开始时间 |
| `endTime` | `end_time` | string | 结束时间 |

## Mock 数据

v0.3 的 MockHttpClient 返回 3 门课程（高等数学、程序设计基础、大学物理），
与 `tests/fixtures/courses.json` 内容一致。

