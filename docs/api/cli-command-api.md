# CLI 命令 API

> 当前仓库版本阶段为 `v0.4.0`。本页列出 CLI 的稳定命令合同和已预留命令面；v0.4 稳定基线承诺 mock/offline 下的 parser、service、cache、命令树、JSON envelope、exit code、config/cache 子命令和 CLI golden 集成测试。真实登录、真实 HTTP 和真实写属于 v0.5+ 后续阶段。

所有命令支持 `--json`，JSON 输出统一为 `{"ok": true, "data": {...}, "error": null}` 或 `{"ok": false, "data": null, "error": {...}}`。列表类命令统一使用 `--page <n>` / `--size <n>`，兼容旧 `--limit <n>` 映射到 `--size <n>`；保留接口不代表真实语义完成，未实现能力必须稳定返回错误。

## 能力状态与安全边界

CLI 暴露只表示验收入口可发现，不表示原 UBAA 后端语义已完成。文档和代码注释统一使用这些状态：`Aligned`、`ReadOnlyCandidate`、`PartiallyMigrated`、`MockOnly`、`Placeholder`、`NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified`。

- `MockOnly` 命令只验证离线合同，不证明真实服务可用。
- `ReadOnlyCandidate` 命令可能触发远端只读请求，仍需处理 session expired、字段漂移和 parse error。
- `WriteGated` 命令会改变远端状态，必须同时满足 typed service、`--confirm` / `--yes` 和平台 `write_operations=true`。
- `Placeholder` / `NotImplemented` 接口必须稳定失败，不读取本地文件、不触发远端请求，除非命令文档明确说明是 typed 写操作的真实上传场景。
- cookie/session/token、验证码、URL query、上传文件名和业务敏感数据不得出现在错误、日志、diagnostics 或测试输出中。

## 基础命令

- `version`
- `help`
- `login --username <id> --password <pw> [--mode vpn|direct] [--mock]`
- `logout --confirm`
- `whoami`
- `user info`
- `app version`
- `app announcement`
- `config show`
- `config set --key <mode|proxy|cache> --value <value> --confirm`
- `cache clear --confirm`

## 教务命令

- `term list`
- `week list`
- `course today`
- `course date --date <yyyy-MM-dd>`
- `course week --week <n>`
- `exam list`
- `grade list [--term <termCode>] [--all]`
- `grade all`
- `classroom query --campus <id> --date <yyyy-MM-dd> [--sections 1,2,3]`

`classroom query --sections` 只返回同时空闲所有指定节次的教室。

## 作业与待办

- `spoc assignments [--pending-only] [--include-expired]`
- `spoc assignment show --id <id>`
- `judge assignments [--course-id <id>] [--include-expired] [--include-history]`
- `judge assignment show --assignment-id <id>`
- `judge assignment details --assignment-id <id>`
- `judge assignment details-batch --input <json|@file>`
- `signin today`
- `signin do --course-id <id> --confirm`
- `evaluation list`
- `evaluation submit [--id <id>] --confirm`
- `todo list [--pending-only|--all]`

`judge assignment details-batch --input` 支持 JSON 数组、`{"assignmentIds": [...]}`、逗号/空白分隔字符串；以 `@` 开头时从文件读取。批量详情是 partial failure contract：命令整体成功时仍可能包含 `status="error"` 的单项记录，`fields.submissionStatusText` 保存稳定错误码，`fields.content` 保存脱敏错误消息；后续上层客户端不得把这类记录过滤成空列表。
`todo list` 聚合 SPOC、Judge、Signin、Evaluation 中仍需处理的项目，不包含历史场馆订单；默认等价于 `--pending-only`，`--all` 会包含非待处理记录和 source-level error。

## 博雅、场馆、图书馆和打卡

- `bykc profile`
- `bykc stats`
- `bykc courses [--page n] [--size n] [--all] [--status <status>] [--category <name>] [--sub-category <name>] [--campus <id>] [--keyword <text>]`
- `bykc chosen`
- `bykc course show --course-id <id>`
- `bykc select --course-id <id> --confirm`
- `bykc unselect --course-id <id> --confirm`
- `bykc sign --course-id <id> --sign-type <1|2> --confirm`
- `cgyy sites`
- `cgyy purpose-types`
- `cgyy day-info --date <date> --site-id <id>`
- `cgyy orders [--page n] [--size n]`
- `cgyy order show --order-id <id>`
- `cgyy order lock-code`
- `cgyy reserve ... --captcha <captcha> --token <token> --confirm`
- `cgyy order cancel --order-id <id> --confirm`
- `libbook libraries --day <date>`
- `libbook areas --library-id <id> --day <date> [--storey-id <id>]`
- `libbook area show --area-id <id>`
- `libbook seats --area-id <id> --day <date> [--start-time HH:mm] [--end-time HH:mm]`
- `libbook reservations [--page n] [--limit n]`
- `libbook book --seat-id <id> --day <date> --segment <segment> --confirm`
- `libbook cancel --booking-id <id> --confirm`
- `ygdk overview`
- `ygdk records [--page n] [--size n|--limit n]`
- `ygdk submit --item-id <id> --start-time <time> --end-time <time> --place <place> --photo <path> [--share] --confirm`

`ygdk submit` 是 `WriteGated` typed 写操作：会读取 `--photo` 指定的本地文件并可能上传图片、改变远端打卡状态；默认平台 `write_operations=false` 时必须失败。

CGYY live smoke 只在 direct 模式下采样 `sites/orders/order lock-code`；非 direct 模式必须显式跳过，不得隐式改路由。`cgyy orders` 和 `cgyy order lock-code` 是只读但高敏感输出，错误和 diagnostics 必须脱敏。

## 保留接口

- `file upload --path <path> --confirm`

`file upload` 是后续附件/文件上传的稳定 CLI 占位接口；当前真实上传语义未证明完整，必须返回 `NotImplemented`，不得读取文件或触发远端请求。它与 `ygdk submit --photo` 的 typed 写上传不同，不能被当作通用上传已完成。

`evaluation submit` 的 JSON 输出包含 `accepted`、`message` 和 `result.fields.results`，其中 `results` 是逐课程提交结果数组的 JSON 字符串。
LibBook、CGYY、YGDK 当前保持兼容 `FeatureRecord` 输出，业务字段位于 `fields`，后续 Native/Harmony 边界可直接按 `id/title/status/fields` 稳定消费。

所有写操作必须显式传入 `--confirm`，且真实写还要求平台 `write_operations=true`。真实登录和只读 live 请求可能保存 cookie/session；平台 secure store 或 cookie persistence 不可用时必须 fail-closed，而不是回退成明文或 volatile 成功。
