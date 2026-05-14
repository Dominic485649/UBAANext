# 测试策略

## 测试层级

| 层级 | 框架 | 范围 |
|------|------|------|
| 单元测试 | Catch2 | Result、AuthService、CourseService |
| 集成测试 | 待定 | 端到端 CLI 命令（v0.2+） |

## v0.1 测试覆盖

### ResultTests

- Result<int>::Ok — 有值、值正确、可转换为 bool
- Result<int>::Fail — 无值、携带错误码和消息
- Result<void>::Ok — 有值、可转换为 bool
- Result<void>::Fail — 无值、携带错误

### AuthServiceTests

- 使用有效凭据 Mock 登录成功
- 使用空用户名登录失败（InvalidArgument）
- 使用空密码登录失败（InvalidArgument）
- 登出清除会话

### CourseServiceTests

- 返回 Mock 课程（数量 > 0）
- 课程具有非空的名称和教室
- 返回至少 3 门课程

## 运行测试

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

## Mock 策略

- v0.1 中所有外部依赖均使用 Mock
- MockSecureStore 使用 std::unordered_map
- MockHttpClient 返回固定响应
- 无真实网络请求，无真实凭据
