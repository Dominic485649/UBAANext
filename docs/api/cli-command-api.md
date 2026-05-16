# CLI 命令 API

所有命令支持 `--json`，JSON 输出统一为 `{"ok": true, "data": {...}, "error": null}` 或 `{"ok": false, "data": null, "error": {...}}`。

## 基础命令

- `version`
- `help`
- `login --username <id> --password <pw> [--mode vpn|direct] [--mock]`
- `logout`
- `whoami`
- `user info`
- `app version`
- `app announcement`
- `config show`
- `config set --key <mode|proxy|cache> --value <value>`
- `cache clear`

## 教务命令

- `term list`
- `week list`
- `course today`
- `course date --date <yyyy-MM-dd>`
- `course week --week <n>`
- `exam list`
- `grade list [--term <termCode>]`
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
- `todo list [--pending-only]`

`judge assignment details-batch --input` 支持 JSON 数组、`{"assignmentIds": [...]}`、逗号/空白分隔字符串；以 `@` 开头时从文件读取。
`todo list` 聚合 SPOC、Judge、Signin、Evaluation 中仍需处理的项目，不包含历史场馆订单。

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
- `ygdk records [--page n] [--size n]`
- `ygdk submit --item-id <id> --start-time <time> --end-time <time> --place <place> --photo <path> [--share] --confirm`

`evaluation submit` 的 JSON 输出包含 `accepted`、`message` 和 `result.fields.results`，其中 `results` 是逐课程提交结果数组的 JSON 字符串。
LibBook、CGYY、YGDK 当前保持兼容 `FeatureRecord` 输出，业务字段位于 `fields`，后续 Native/Harmony 边界可直接按 `id/title/status/fields` 稳定消费。

所有写操作必须显式传入 `--confirm`。
