# 教室数据计划

## 概述

教室数据协议定义了 UBAA 教室 API 的请求/响应格式。
当前 v0.4 仍使用 Mock HTTP 返回硬编码 JSON，真实 API 对接进入 v0.5+。

## API 端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/classroom/query` | GET | 查询空闲教室 |

## 响应格式

JSON 对象，按教学楼分组：

```json
{
  "buildings": {
    "J3": [
      {
        "id": "J3-101",
        "name": "J3-101",
        "floorId": "1",
        "freeSections": [1, 2, 3, 4]
      }
    ]
  }
}
```

## 字段映射

| JSON 键 | Model 字段 | 类型 | 说明 |
|---------|-----------|------|------|
| `buildings` | `buildings` | `map<string, vector<ClassroomInfo>>` | 按楼分组 |
| `id` | `id` | string | 教室标识符 |
| `name` | `name` | string | 教室名称 |
| `floorId` | `floor_id` | string | 楼层标识 |
| `freeSections` | `free_sections` | `vector<int>` | 空闲节次列表 |

## Mock 数据

v0.3 的 MockHttpClient 返回 J3 教学楼 3 间教室（101/202/303），
分别覆盖上午、下午、晚上的空闲时段，
与 `tests/fixtures/classrooms.json` 内容一致。

