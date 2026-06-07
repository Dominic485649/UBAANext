# JSON 输出格式

UBAA Next CLI 的 `--json` 输出遵循统一 JSON envelope。C ABI 业务 API 也复用同一 envelope 语义；NAPI/ArkTS wrapper 应保持字段命名和错误码一致。

## 统一格式

### 成功响应

```json
{
  "ok": true,
  "data": {},
  "error": null
}
```

### 失败响应

```json
{
  "ok": false,
  "data": null,
  "error": {
    "code": "InvalidArgument",
    "message": "错误描述"
  }
}
```

`error.code` 必须来自 Core `ErrorCode` / `error_code_to_string(...)`。`error.message` 必须已脱敏，不得包含 password、cookie、token、ticket、Authorization、Set-Cookie、URL query、raw HTML、本地路径或上传文件名等敏感原文。

## 基础命令

### version

```json
{
  "ok": true,
  "data": {
    "version": "0.4.0"
  },
  "error": null
}
```

### capability show

```json
{
  "ok": true,
  "data": {
    "capabilities": {
      "realNetwork": true,
      "secureCookiePersistence": false,
      "cookiePersistence": false,
      "redirectControl": true,
      "opensslCrypto": true,
      "secureStore": false,
      "appDataPath": true,
      "uploadBytes": true,
      "liveLogin": false,
      "writeOperations": true
    }
  },
  "error": null
}
```

Capability 是宿主能力声明，不代表真实登录、真实写 UI 或业务 API 已完成。`writeOperations=true` 仍必须通过显式确认和 Core `WriteOperationGate`。

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
        "name": "capability show",
        "description": "显示当前平台能力声明；capability 不代表真实登录、真实写 UI 或业务 API 完成"
      }
    ],
    "version": "0.4.0"
  },
  "error": null
}
```

## 账号与会话

### login

`login --json` 可能连续输出消息对象和账户对象；调用方应按行或按连续 JSON 对象消费。

```json
{
  "ok": true,
  "data": {
    "message": "登录成功。"
  },
  "error": null
}
```

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

## 只读业务命令

### course today / course date / course week

```json
{
  "ok": true,
  "data": {
    "courses": [
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
    ]
  },
  "error": null
}
```

### exam list

```json
{
  "ok": true,
  "data": {
    "exams": [
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
    ]
  },
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
  "data": {
    "terms": [
      {
        "code": "2025-2026-1",
        "name": "2025-2026学年第一学期",
        "selected": false,
        "index": 0
      }
    ]
  },
  "error": null
}
```

### week list

```json
{
  "ok": true,
  "data": {
    "weeks": [
      {
        "serialNumber": 1,
        "name": "第1周",
        "startDate": "2026-02-23",
        "endDate": "2026-03-01",
        "isCurrent": false
      }
    ]
  },
  "error": null
}
```

### grade list / grade all

```json
{
  "ok": true,
  "data": {
    "grades": [
      {
        "courseName": "高等数学",
        "score": "95",
        "credit": "3.0",
        "gradePoint": "4.0",
        "termCode": "2025-2026-1"
      }
    ]
  },
  "error": null
}
```

### live resources / detail / download

`live resources` 输出课堂资源列表，`courseId/subId` 位于 `fields`，后续 `live detail/download` 使用这两个字段：

```json
{
  "ok": true,
  "data": {
    "resources": [
      {
        "id": "course-1:sub-1",
        "title": "计算机网络",
        "status": "回放",
        "fields": {
          "courseId": "course-1",
          "subId": "sub-1",
          "teacher": "李老师",
          "room": "J3-101",
          "timeSlot": "1-2",
          "timeRange": "08:00-09:40"
        }
      }
    ]
  },
  "error": null
}
```

`live detail` 输出单个 `resource`，包括视频和 PPT 候选：

```json
{
  "ok": true,
  "data": {
    "resource": {
      "id": "course-1:sub-1",
      "title": "计算机网络",
      "status": "video",
      "fields": {
        "courseId": "course-1",
        "subId": "sub-1",
        "hasVideo": "true",
        "primaryVideoUrl": "https://example.invalid/video.mp4",
        "primaryVideoHls": "false",
        "pptResourceGuid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "pptGuids": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa,bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      }
    }
  },
  "error": null
}
```

`live download` 输出 `downloads[]`。`status:"partial"` 表示至少一个目标成功，或 HLS 无法合并时已写入 `.m3u8.url` sidecar；`pptxPath`、`videoPath`、`hlsSidecarPath` 是本地路径，日志和错误输出必须按敏感路径策略脱敏。

```json
{
  "ok": true,
  "data": {
    "downloads": [
      {
        "id": "course-1:sub-1",
        "title": "计算机网络",
        "status": "partial",
        "fields": {
          "usedGuid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "slides": "24",
          "images": "23",
          "failedImages": "1",
          "pptxPath": "D:\\tmp\\live\\2026-06-01_course-1_sub-1.pptx",
          "videoHls": "true",
          "hlsSidecarPath": "D:\\tmp\\live\\2026-06-01_course-1_sub-1.m3u8.url"
        }
      }
    ]
  },
  "error": null
}
```

## FeatureRecord 与 partial failure

聚合类接口使用 `FeatureRecord` 展示兼容数据：

```json
{
  "id": "todo:library",
  "title": "图书馆预约",
  "status": "pending",
  "fields": {
    "source": "libbook",
    "dueTime": "2026-06-04 18:00"
  }
}
```

如果下游来源失败，但整体接口仍能返回部分结果，envelope 保持 `ok:true`，失败来源以 `status:"error"` 单项表达：

```json
{
  "ok": true,
  "data": {
    "todos": [
      {
        "id": "todo:course:1",
        "title": "课程签到",
        "status": "pending",
        "fields": {
          "source": "signin"
        }
      },
      {
        "id": "todo:judge:source-error",
        "title": "Judge 作业",
        "status": "error",
        "fields": {
          "type": "source-error",
          "errorCode": "NetworkError",
          "errorMessage": "下游服务暂不可用",
          "submissionStatus": "error"
        }
      }
    ]
  },
  "error": null
}
```

CLI、C ABI 和未来 NAPI/ArkTS 不得过滤 `status:"error"` 项；UI 应展示局部失败提示，同时保留成功项。

## 配置与缓存

### config show

```json
{
  "ok": true,
  "data": {
    "mode": "vpn",
    "proxy": "",
    "cacheEnabled": true,
    "sessionPath": "[REDACTED]",
    "cookiePath": "[REDACTED]",
    "configPath": "[REDACTED]",
    "version": "0.4.0"
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
| --- | --- |
| `None` | 无错误 |
| `Unknown` | 未分类错误 |
| `InvalidArgument` | 无效参数 |
| `NetworkError` | 网络错误 |
| `AuthFailed` | 认证失败 |
| `SessionExpired` | 会话过期 |
| `ParseError` | 解析错误 |
| `UnsupportedPlatform` | 平台不支持 |
| `UnsupportedNetwork` | 平台未接入网络能力 |
| `UnsupportedSecureStore` | 平台未接入安全存储 |
| `UnsupportedCrypto` | 平台未接入加密能力 |
| `UnsupportedCookiePersistence` | 平台未接入 Cookie 持久化 |
| `Timeout` | 操作超时 |
| `TlsError` | TLS/证书错误 |
| `CryptoError` | 加密运算错误 |
| `StorageError` | 存储错误 |
| `NotImplemented` | 未实现 |

## 字段命名规范

- CLI JSON 与 ArkTS/TypeScript 保持 camelCase。
- C ABI `UbaaNextCapabilities` 使用 C ABI snake_case struct 字段，但 JSON / ArkTS capability 使用 camelCase。
- 列表类业务数据必须放在 `data.<key>` 中，例如 `data.courses`、`data.terms`、`data.weeks`、`data.todos`。
