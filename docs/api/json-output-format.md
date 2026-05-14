# JSON 输出格式

UBAA Next CLI 的 `--json` 输出遵循统一的 JSON 格式规范。

## 统一格式

### 成功响应

```json
{
  "ok": true,
  "data": { ... },
  "error": null
}
```

### 失败响应

```json
{
  "ok": false,
  "data": null,
  "error": {
    "code": "ErrorCode",
    "message": "错误描述"
  }
}
```

## 命令输出格式

### version

```json
{
  "ok": true,
  "data": {
    "version": "0.3.0"
  },
  "error": null
}
```

### help

```json
{
  "ok": true,
  "data": {
    "commands": [
      {
        "name": "version",
        "description": "显示版本信息"
      },
      {
        "name": "login",
        "description": "登录",
        "options": [
          {
            "name": "--username",
            "description": "学号",
            "required": true
          }
        ]
      }
    ],
    "version": "0.3.0"
  },
  "error": null
}
```

### login

输出两个 JSON 对象：

1. 消息对象：
```json
{
  "ok": true,
  "data": {
    "message": "登录成功。"
  },
  "error": null
}
```

2. 账户对象：
```json
{
  "ok": true,
  "data": {
    "studentId": "20260000",
    "displayName": "张三"
  },
  "error": null
}
```

### whoami

```json
{
  "ok": true,
  "data": {
    "studentId": "20260000",
    "displayName": "张三"
  },
  "error": null
}
```

### logout

```json
{
  "ok": true,
  "data": {
    "message": "已登出。"
  },
  "error": null
}
```

### course today / date / week

```json
{
  "ok": true,
  "data": [
    {
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
  ],
  "error": null
}
```

### exam list

```json
{
  "ok": true,
  "data": [
    {
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
  ],
  "error": null
}
```

### classroom query

```json
{
  "ok": true,
  "data": {
    "buildings": {
      "J3": [
        {
          "name": "J3-101",
          "floorId": "1",
          "freeSections": [1, 2, 3, 4]
        }
      ]
    }
  },
  "error": null
}
```

### term list

```json
{
  "ok": true,
  "data": [
    {
      "code": "2025-2026-1",
      "name": "2025-2026学年第一学期",
      "selected": false,
      "index": 0
    }
  ],
  "error": null
}
```

### week list

```json
{
  "ok": true,
  "data": [
    {
      "serialNumber": 1,
      "name": "第1周",
      "startDate": "2026-02-23",
      "endDate": "2026-03-01",
      "isCurrent": false
    }
  ],
  "error": null
}
```

### config show

```json
{
  "ok": true,
  "data": {
    "mode": "vpn",
    "proxy": "",
    "cacheEnabled": true,
    "sessionPath": "C:\\Users\\...\\session.dat",
    "cookiePath": "C:\\Users\\...\\cookies.dat",
    "configPath": "C:\\Users\\...\\config.json",
    "version": "0.3.0"
  },
  "error": null
}
```

### cache clear

```json
{
  "ok": true,
  "data": {
    "message": "缓存已清除。"
  },
  "error": null
}
```

## 错误码

| 错误码 | 说明 |
|--------|------|
| `None` | 无错误 |
| `Unknown` | 未分类错误 |
| `InvalidArgument` | 无效参数 |
| `NetworkError` | 网络错误 |
| `AuthFailed` | 认证失败 |
| `SessionExpired` | 会话过期 |
| `ParseError` | 解析错误 |
| `NotImplemented` | 未实现 |

## 字段命名规范

- 使用 camelCase 命名（小驼峰）
- 与 ArkTS/TypeScript 惯例一致
- 例如：`studentId`、`courseName`、`weekStart`
