# 考试数据计划

## 概述

考试数据协议定义了 UBAA 考试 API 的请求/响应格式。
当前 v0.4 仍使用 Mock HTTP 返回硬编码 JSON，真实 API 对接进入 v0.5+。

## API 端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/exam/list` | GET | 获取考试列表 |

## 响应格式

JSON 数组，每个元素代表一场已排考试：

```json
[
  {
    "id": "EXAM001",
    "courseName": "高等数学",
    "location": "J3-101",
    "timeText": "2026-06-20 09:00-11:00",
    "courseNo": "MATH101",
    "examDate": "2026-06-20",
    "startTime": "09:00",
    "endTime": "11:00",
    "seatNo": "15",
    "examType": "期末考试",
    "status": 1
  }
]
```

## 字段映射

| JSON 键 | Model 字段 | 类型 | 说明 |
|---------|-----------|------|------|
| `id` | `id` | string | 唯一考试 ID |
| `courseName` | `course_name` | string | 考试科目名称 |
| `location` | `location` | string | 考场 |
| `timeText` | `time_text` | string | 人类可读时间 |
| `courseNo` | `course_no` | string | 课程代码 |
| `examDate` | `exam_date` | string | 考试日期 |
| `startTime` | `start_time` | string | 开始时间 |
| `endTime` | `end_time` | string | 结束时间 |
| `seatNo` | `seat_no` | string | 座位号 |
| `examType` | `exam_type` | string | 考试类型 |
| `status` | `status` | ExamStatus | 状态枚举 |

## 状态枚举

| 值 | 枚举 | 说明 |
|----|------|------|
| 0 | `Pending` | 待安排 |
| 1 | `Arranged` | 已安排 |
| 2 | `Finished` | 已结束 |

## Mock 数据

v0.3 的 MockHttpClient 返回 3 场考试（高等数学、程序设计基础、大学物理），
与 `tests/fixtures/exams.json` 内容一致。

