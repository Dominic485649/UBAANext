# TD 协议说明

本文记录 UBAANext TD 协议层当前实现，用于维护 `Protocol::Td` 与 `platform/common/tcp` 的边界。

## Transport / Client 分层

- `Protocol::Td::ITdTransport` 只负责发送一个完整请求帧并返回完整响应帧。
- `Protocol::Td::TdProtocolClient` 负责构造 TD payload、编码帧、调用 transport、解码响应和解析业务字段。
- 测试必须注入 mock transport 或 mock client；生产 CLI 才实例化 `Platform::Tcp::TdTcpTransport`。

## 帧格式

TD 帧头固定 5 字节：

```text
[length: uint32 big-endian][request_type: uint8][body...]
```

`length` 是 body 字节数，不包含 5 字节头。

请求类型：

| request type | 用途 |
| :---: | :--- |
| `80` | check 请求，body 为 JSON。 |
| `100` | photo 上传请求，body 为 `machinesn_timestamp_ms` 前缀加图片 bytes。 |

响应帧使用相同头格式。解码时必须验证总长度与 header length 一致，否则返回 `ParseError`。

## Check payload

check payload 来自 `build_check_data`，核心字段包括：

- `cardno`：用户 `card_id`，大写。
- `userno`：学号，大写。
- `machine` / machine serial 相关字段：来自 TD config 的机器映射。
- `schoolno`、`eventno`、`type`：来自 TD config。
- 时间戳：调用方传入的毫秒时间戳。

`student_id`、`card_id` 和机器配置缺失都会返回 `InvalidArgument`。

## Photo payload

photo payload 来自 `build_photo_payload`：

```text
<machinesn>_<timestamp_ms><photo bytes>
```

图片 bytes 来自 TD store 中已复制的本地图片。协议层不负责读取文件；文件读取在服务层通过 `TdStore::image_path` 与普通文件读取完成。

## 响应解析

`parse_raw_response` 和 `parse_check_response` 会解析 JSON 响应并清洗服务器消息。`extract_exercise_count` 从 `srvresp` 文本中识别本学期锻炼次数。

典型异常：

| 场景 | 错误码 |
| :--- | :--- |
| 帧头过短或长度不一致 | `ParseError` |
| JSON 无法解析或结构不符合预期 | `ParseError` |
| endpoint 缺少 IP 或端口非法 | `InvalidArgument` |
| TCP connect / send / recv 失败 | `NetworkError` |
| socket timeout | `Timeout` |

## 安全约束

TD check、photo upload 和 query count 均按 WriteGated 远端边界处理。即使 `query_count` 表面上是查询，也复用 TD check 协议并可能改变服务器侧记录，因此 CLI 必须要求确认，Core service 必须要求 `WriteOperationGate`。

协议日志和错误不得包含图片 bytes、原始帧、`.env` 内容、密码、cookie 或 token。需要诊断时只报告错误类别、状态摘要和已脱敏消息。
