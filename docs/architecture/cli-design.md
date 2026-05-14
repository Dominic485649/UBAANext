# CLI 设计

## v0.1 CLI 命令

### `ubaa version`

输出当前版本字符串。

```
UBAA Next 0.1.0
```

### `ubaa login --mock --username <id> --password <pw>`

执行 Mock 登录。

```
Login succeeded.
User: Test User
Student ID: 20260000
```

### `ubaa course today --mock`

显示今天的 Mock 课程表。

```
Today Courses:
1. 高等数学 | 1-2节 | J3-101
2. 程序设计基础 | 3-4节 | J3-202
3. 大学物理 | 5-6节 | J3-303
```

## 参数解析

v0.1 使用手写参数解析，不使用外部 CLI 库。
