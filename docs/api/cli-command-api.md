# CLI 命令 API

> 当前仓库版本阶段为 `v0.4.0`。本页列出 CLI 的稳定命令合同和已预留命令面；v0.4 稳定基线承诺 mock/offline 下的 parser、service、cache、命令树、JSON envelope、exit code、config/cache 子命令和 CLI golden 集成测试。真实登录、真实 HTTP 和真实写属于 v0.5+ 后续阶段。

所有命令支持 `--json`，JSON 输出统一为 `{"ok": true, "data": {...}, "error": null}` 或 `{"ok": false, "data": null, "error": {...}}`。列表类命令统一使用 `--page <n>` / `--size <n>`，兼容旧 `--limit <n>` 映射到 `--size <n>`；保留接口不代表真实语义完成，未实现能力必须稳定返回错误。`help --json` 保持 `commands[].name`、`commands[].description`、`commands[].options` 向后兼容，并可在 option 上附加 `placeholder`、`sourceCommand`、`sourceField`、`example`、`note` 等来源元数据，供 GUI、脚本或文档生成器解释 `<...>` 占位符。

不加 `--json` 时，人类可读输出采用 PowerShell 风格表格，包含标题、列名、分隔线和对齐后的数据行；列表类命令会显式显示 `Id` 列，供用户复制到后续 `<...-id>` 参数。文本表格是终端展示格式，不是机器解析合同；自动化调用必须使用 `--json` 并读取 `data.<key>[].id` 或下表列出的例外字段。

## 能力状态与安全边界

CLI 暴露只表示验收入口可发现，不表示原 UBAA 后端语义已完成。文档和代码注释统一使用这些状态：`Aligned`、`ReadOnlyCandidate`、`PartiallyMigrated`、`MockOnly`、`Placeholder`、`NotImplemented`、`Unsupported`、`Fallback`、`WriteGated`、`Unverified`。

- `MockOnly` 命令只验证离线合同，不证明真实服务可用。
- `ReadOnlyCandidate` 命令可能触发远端只读请求，仍需处理 session expired、字段漂移和 parse error。
- `WriteGated` 命令会改变远端状态；Windows、Linux、Harmony 平台默认 `write_operations=true`，但 CLI 仍要求每条写命令通过 `--confirm` / `--yes` / `-y` 或人类可读模式交互输入 `y` 完成确认，`--json` / 脚本模式缺少确认时必须返回 `InvalidArgument`。
- `Placeholder` / `NotImplemented` 接口必须稳定失败，不读取本地文件、不触发远端请求，除非命令文档明确说明是 typed 写操作的真实上传场景。
- cookie/session/token、验证码、URL query、上传文件名和业务敏感数据不得出现在错误、日志、diagnostics 或测试输出中。

## 参数占位符来源约定

`<...>` 表示调用时需要替换的值，不是字面量。资源 ID 默认来自对应列表命令人类可读表格的 `Id` 列；JSON 模式下对应 `data.<key>[].id` 字段，例外字段会在 help metadata 和下表中明确标出。

| 占位符 | 来源命令 | 来源字段 | 常见消费者命令 |
| :--- | :--- | :--- | :--- |
| `<assignment-id>` | `spoc assignments` / `judge assignments` | `id` | `spoc assignment show`、`judge assignment show/details` |
| `<signin-id>` | `signin today` | `id` | `signin do --id` |
| `<course-id>` | `bykc courses` / `bykc chosen` | `id` 或 `fields.courseId` | `bykc course show/select/unselect/sign` |
| `<site-id>` | `cgyy sites` | `id` | `cgyy day-info`、`cgyy reserve` |
| `<space-id>` | `cgyy day-info` | `id` | `cgyy reserve` |
| `<time-id>` | `cgyy day-info` | `fields.timeId` | `cgyy reserve --id` |
| `<purpose-type-id>` | `cgyy purpose-types` | `id` | `cgyy reserve --purpose-type` |
| `<order-id>` | `cgyy orders` | `id` | `cgyy order show/cancel` |
| `<library-id>` | `libbook libraries` | `id` | `libbook areas` |
| `<area-id>` | `libbook areas` | `id` | `libbook area show`、`libbook seats` |
| `<seat-id>` | `libbook seats` | `id` | `libbook book` |
| `<booking-id>` | `libbook reservations` | `id` | `libbook cancel` |
| `<item-id>` | `ygdk overview` | `status=item` 记录的 `id` | `ygdk submit` |
| `<evaluation-id>` | `evaluation list` | `id` | `evaluation submit` |

## 基础命令

- `version`
- `help`
- `login <账号> <密码> [--mode vpn|direct] [--mock]`，兼容 `login --username <账号> --password <密码>`
- `logout [-y|--confirm|--yes]`
- `whoami`
- `user info`
- `app version`
- `app announcement`
- `config show`
- `config set --key <mode|proxy|cache> --value <value> [-y|--confirm|--yes]`
- `cache clear [-y|--confirm|--yes]`

## 教务命令

- `term list`
- `week list`
- `course today`
- `course date --date <yyyy-MM-dd>`
- `course week --week <n>`
- `exam list`
- `grade list [--term <termCode>] [--all]`
- `grade all`
- `classroom query --campus <campus-id> --date <yyyy-MM-dd> [--sections 1,2,3]`

`classroom query --sections` 只返回同时空闲所有指定节次的教室。

## 作业与待办

- `spoc assignments [--pending-only] [--include-expired]`
- `spoc assignment show --id <assignment-id>`
- `judge assignments [--course-id <course-id>] [--include-expired] [--include-history]`
- `judge assignment show --assignment-id <assignment-id>`
- `judge assignment details --assignment-id <assignment-id>`
- `judge assignment details-batch --input <json|@file|ids>`
- `signin today`
- `signin do [--id <signin-id>|--course-id <course-id>] [-y|--confirm|--yes]`
- `evaluation list`
- `evaluation submit [--id <evaluation-id>] [-y|--confirm|--yes]`
- `todo list [--pending-only|--all]`

`judge assignment details-batch --input` 支持 JSON 数组、`{"assignmentIds": [...]}`、逗号/空白分隔字符串；以 `@` 开头时从文件读取。批量详情是 partial failure contract：命令整体成功时仍可能包含 `status="error"` 的单项记录，`fields.submissionStatusText` 保存稳定错误码，`fields.content` 保存脱敏错误消息；后续上层客户端不得把这类记录过滤成空列表。
`todo list` 聚合 SPOC、Judge、Signin、Evaluation 中仍需处理的项目，不包含历史场馆订单；默认等价于 `--pending-only`，`--all` 会包含非待处理记录和 source-level error。

## 博雅、场馆、图书馆和打卡

- `bykc profile`
- `bykc stats`
- `bykc courses [--page <n>] [--size <n>] [--all] [--status <status>] [--category <name>] [--sub-category <name>] [--campus <campus-id>] [--keyword <text>]`
- `bykc chosen`
- `bykc course show --course-id <course-id>`
- `bykc select --course-id <course-id> [-y|--confirm|--yes]`
- `bykc unselect --course-id <course-id> [-y|--confirm|--yes]`
- `bykc sign --course-id <course-id> --sign-type <1|2> [-y|--confirm|--yes]`
- `cgyy sites`
- `cgyy purpose-types`
- `cgyy day-info --date <yyyy-MM-dd> --site-id <site-id>`
- `cgyy orders [--page <n>] [--size <n>]`
- `cgyy order show --order-id <order-id>`
- `cgyy order lock-code [--order-id <order-id>]`
- `cgyy reserve --site-id <site-id> --space-id <space-id> --id <time-id> --date <yyyy-MM-dd> --purpose-type <purpose-type-id> --theme <theme> --phone <phone> --joiners <joiners> --captcha <captcha> --token <token> [-y|--confirm|--yes]`
- `cgyy order cancel --order-id <order-id> [-y|--confirm|--yes]`
- `libbook libraries`
- `libbook areas --library-id <library-id> [--date <yyyy-MM-dd>] [--storey-id <storey-id>]`
- `libbook area show --area-id <area-id>`
- `libbook seats --area-id <area-id> [--date <yyyy-MM-dd>] [--start-time <HH:mm>] [--end-time <HH:mm>]`
- `libbook reservations [--page <n>] [--size <n>]`
- `libbook book --seat-id <seat-id> --date <yyyy-MM-dd> [--segment <segment>|--start-time <HH:mm> --end-time <HH:mm>] [-y|--confirm|--yes]`
- `libbook cancel --booking-id <booking-id> [-y|--confirm|--yes]`
- `ygdk overview`
- `ygdk records [--page <n>] [--size <n>|--limit <n>]`
- `ygdk submit [--item-id <item-id>] --start-time <time> --end-time <time> --place <place> --photo <path> [--share] [-y|--confirm|--yes]`

`ygdk submit` 是 `WriteGated` typed 写操作：会读取 `--photo` 指定的本地文件并可能上传图片、改变远端打卡状态；Windows、Linux、Harmony 平台默认具备写能力，但仍必须逐次确认。

CGYY live smoke 只在 direct 模式下采样 `sites/orders/order lock-code`；非 direct 模式必须显式跳过，不得隐式改路由。`cgyy orders` 和 `cgyy order lock-code` 是只读但高敏感输出，错误和 diagnostics 必须脱敏。

## 保留接口

- `file upload --path <path> [-y|--confirm|--yes]`

`file upload` 是后续附件/文件上传的稳定 CLI 占位接口；当前真实上传语义未证明完整，必须返回 `NotImplemented`，不得读取文件或触发远端请求。它与 `ygdk submit --photo` 的 typed 写上传不同，不能被当作通用上传已完成。

`evaluation submit` 的 JSON 输出包含 `accepted`、`message` 和 `result.fields.results`，其中 `results` 是逐课程提交结果数组的 JSON 字符串。
LibBook、CGYY、YGDK 当前保持兼容 `FeatureRecord` 输出，业务字段位于 `fields`，后续 Native/Harmony 边界可直接按 `id/title/status/fields` 稳定消费。

所有写操作都必须逐次确认：可显式传入 `--confirm`、`--yes` 或 `-y`；未传时，人类可读模式会询问 `y/N`，`--json` / 脚本模式会 fail-closed 并返回 `InvalidArgument`。真实登录和只读 live 请求可能保存 cookie/session；平台 secure store 或 cookie persistence 不可用时必须 fail-closed，而不是回退成明文或 volatile 成功。
