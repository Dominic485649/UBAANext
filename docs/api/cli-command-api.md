# CLI 命令 API

> 当前仓库版本阶段为 `v0.4.0`，`master` 另包含 post-0.4 的 `reference/buaa-api` 功能迁移。v0.4 稳定基线承诺 mock/offline 下的 parser、service、cache、命令树、JSON envelope、exit code、config/cache 子命令和 CLI golden 集成测试；Cloud/SPOC/Signin/SRS/Evaluation/WiFi 等真实协议入口以本文和 `docs/reports/buaa-api-migration-matrix.md` 为准。

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
| `<evaluation-id>` | `evaluation list` | `id` | `evaluation form`、`evaluation submit`、`evaluation form submit` |
| `<live-id>` | `live week` | `id` 或 `fields.liveId` | 周课表直播入口标识 |
| `<live-course-id>` | `live resources` | `fields.courseId` | `live detail/download --course-id` |
| `<live-sub-id>` | `live resources` | `fields.subId` | `live detail/download --sub-id` |
| `<ppt-guid>` | `live detail` | `fields.pptResourceGuid` 或 `fields.pptGuids` | `live download --guid/--alt-guids` |
| `<cloud-docid>` | `file roots` / `file root` / `file list` | `id` | `file list`、`file size`、`file mkdir/rename/move/copy/delete/recycle-* /share-* /download-url/upload` |
| `<share-token>` | 外部云盘分享链接或授权响应 | 不落盘，仅作为请求头 | `file list --token`、`file size --token` |

## 基础命令

- `version`
- `help`
- `login <账号> <密码> [--mode vpn|direct] [--mock]`，兼容 `login --username <账号> --password <密码>`；登录成功后默认保存账号密码：Windows 使用 DPAPI，Linux 启用 libsecret 时使用 Secret Service；未启用时使用本地加密文件 fallback（`secure_store=false`），必要时允许明文 fallback 并提示风险
- `relogin [-y|--confirm|--yes] [--mode vpn|direct] [--mock]`，复用上一次 `login` 保存的账号密码重新登录并替换本地会话；不再接收账号密码或 `--saved`
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
- `live week --start-date <yyyy-MM-dd> --end-date <yyyy-MM-dd>`
- `live resources --date <yyyy-MM-dd> [--from-course] [--status live|playback|generating|all]`
- `live detail --course-id <live-course-id> --sub-id <live-sub-id> [--date <yyyy-MM-dd>]`
- `live download --date <yyyy-MM-dd> --out-dir <dir> [--include ppt,video] [--overwrite]`
- `live download --course-id <live-course-id> --sub-id <live-sub-id> --out-dir <dir> [--guid <ppt-guid>] [--alt-guids <csv>] [--include ppt,video] [--overwrite]`

`classroom query --sections` 只返回同时空闲所有指定节次的教室。
`live week` 是从 `buaa-api` 的课堂直播周课表迁移而来的 `ReadOnlyCandidate`：Core 通过 `IHttpClient` 请求 `https://yjapi.msa.buaa.edu.cn/courseapi/v2/schedule/get-week-schedules`，并只在 Core 内解析业务 JSON；平台相关 cookie、WebVPN、HTTP backend 和安全存储仍由 Platform/CLI 注入。JSON 输出位于 `data.schedules[]`，记录字段包括 `id`、`title`、`status` 以及 `fields.courseId`、`fields.liveId`、`fields.teacher`、`fields.day`。`--mock` 只验证 CLI envelope 和命令树，不代表真实课堂直播后端语义通过。

`live resources/detail/download` 是按 `reference/BBUAA` 补齐的课堂资源入口，归属于 `live` 而不是 `course`：`course` 继续表示教务课表，`live` 表示课堂直播/回放/PPT 资源。`live resources` 按日期搜索资源，可用 `--from-course` 将结果与当天教务课表做名称或课程号过滤；`--status` 支持 `live`、`playback`、`generating`、`all`。`live detail` 会解析课堂详情、livingroom HTML/JS、视频 URL、HLS 标记、PPT GUID 候选和 PPT 视频候选。`live download` 会下载 PPT 时间轴图片并生成 16:9 `.pptx`，视频为 MP4 直链时流式落盘，为 HLS/m3u8 时尝试调用 `ffmpeg` 合并，找不到或合并失败则写入 `.m3u8.url` sidecar 并将记录标记为 `partial`。输出文件名由日期、课程名、courseId、subId 安全化组成；默认拒绝覆盖，需显式 `--overwrite`。

## 作业与待办

- `spoc assignments [--pending-only] [--include-expired]`
- `spoc assignment show --id <assignment-id>`
- `spoc week`
- `spoc schedule --start-date <yyyy-MM-dd> [--end-date <yyyy-MM-dd>]`
- `spoc courses --term <term-code>`
- `spoc homework submit --id <assignment-id> --course-id <course-id> --file-id <file-id> --name <file-name> [-y|--confirm|--yes]`
- `judge assignments [--course-id <course-id>] [--include-expired] [--include-history]`
- `judge assignment show --assignment-id <assignment-id>`
- `judge assignment details --assignment-id <assignment-id>`
- `judge assignment details-batch --input <json|@file|ids>`
- `signin today`
- `signin schedule --date <yyyy-MM-dd>`
- `signin courses --term <term-code>`
- `signin course schedule --course-id <course-id>`
- `signin do [--id <signin-id>|--course-id <course-id>] [-y|--confirm|--yes]`
- `evaluation list`
- `evaluation form --id <evaluation-id>`
- `evaluation submit [--id <evaluation-id>] [-y|--confirm|--yes]`
- `evaluation form submit --id <evaluation-id> [--reason <text>] [-y|--confirm|--yes]`
- `todo list [--pending-only|--all]`

`judge assignment details-batch --input` 支持 JSON 数组、`{"assignmentIds": [...]}`、逗号/空白分隔字符串；以 `@` 开头时从文件读取。批量详情是 partial failure contract：命令整体成功时仍可能包含 `status="error"` 的单项记录，`fields.submissionStatusText` 保存稳定错误码，`fields.content` 保存脱敏错误消息；后续上层客户端不得把这类记录过滤成空列表。
`todo list` 聚合 SPOC、Judge、Signin、Evaluation 中仍需处理的项目，不包含历史场馆订单；默认等价于 `--pending-only`，`--all` 会包含非待处理记录和 source-level error。
`spoc homework submit`、`signin do`、`evaluation submit` 和 `evaluation form submit` 均为 `WriteGated`。`evaluation form` 返回 `data.evaluation`，字段含 `questionCount`、`choiceCount`、`rwid`、`wjid`、`xnxq`，用于提交前核对；真实评教提交不会自动 smoke。

## 博雅、场馆、图书馆和打卡

- `bykc profile`
- `bykc stats`
- `bykc courses [--page <n>] [--size <n>] [--all] [--status <status>] [--category <name>] [--sub-category <name>] [--campus <campus-id>] [--keyword <text>]`
- `bykc chosen`
- `bykc course show --course-id <course-id>`
- `bykc select --course-id <course-id> [-y|--confirm|--yes]`
- `bykc unselect --course-id <course-id> [-y|--confirm|--yes]`
- `bykc sign --course-id <course-id> --sign-type <1|2> --lat <lat> --lng <lng> [-y|--confirm|--yes]`
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
`bykc sign` 必须由用户显式提供真实坐标；Core 和 CLI 不会根据课程签到范围随机生成或伪造位置。真实选课、退选、签到和签退不进入默认 live smoke。

CGYY live smoke 只在 direct 模式下采样 `sites/orders/order lock-code`；非 direct 模式必须显式跳过，不得隐式改路由。`cgyy orders` 和 `cgyy order lock-code` 是只读但高敏感输出，错误和 diagnostics 必须脱敏。

## SRS 选课与 WiFi

- `srs config`
- `srs batch`
- `srs course query [--scope <scope>] [--page <n>] [--size <n>] [--campus <campus-id>] [--keyword <text>] [--requirement <code>]`
- `srs preselected`
- `srs selected`
- `srs course preselect --id <clazzId> --scope <scope> --token <secretVal> --batch-id <batchId> --index <n> [-y|--confirm|--yes]`
- `srs course select --id <clazzId> --scope <scope> --token <secretVal> [-y|--confirm|--yes]`
- `srs course drop --id <clazzId> --scope <scope> --token <secretVal> [-y|--confirm|--yes]`
- `wifi login [--username <account>] [--password <password>] [-y|--confirm|--yes]`
- `wifi logout [--username <account>] [-y|--confirm|--yes]`

`srs course query` 输出 `data.srsCourses[]`，后续写命令使用记录 `id` 作为 `clazzId`，并读取 `fields.secretVal` 作为 `--token`。`srs batch` 输出 `data.srsBatch`，其中 `id` 可作为 `--batch-id`。SRS 预选、正选和退选会改变真实选课状态，默认 live smoke 不自动执行。

`wifi login/logout` 迁移自 `buaa-api` WiFi 模块。Core 使用 `INetworkEnvironment` 检测校园网与本机 IPv4，Windows/Linux 由平台 adapter 实现，未知平台 fail-closed；认证算法使用 Core 级 `hmac-md5`、`xencode` 和注入的 SHA1 provider。WiFi 登录/登出是真实网关写操作，不会自动 smoke。

## 北航云盘

- `file roots [--root all|user|shared|department|group]`
- `file root`
- `file list --id <cloud-docid> [--token <share-token>]`
- `file size --id <cloud-docid> [--token <share-token>]`
- `file recycle`
- `file shares`
- `file suggest-name --parent-id <cloud-docid> --name <name>`
- `file mkdir --parent-id <cloud-docid> --name <name> [-y|--confirm|--yes]`
- `file rename --id <cloud-docid> --name <name> [-y|--confirm|--yes]`
- `file move --id <cloud-docid> --dest-id <cloud-docid> [-y|--confirm|--yes]`
- `file copy --id <cloud-docid> --dest-id <cloud-docid> [--is-dir] [--token <share-token>] [-y|--confirm|--yes]`
- `file delete --id <cloud-docid> [-y|--confirm|--yes]`
- `file recycle-delete --id <cloud-docid> [-y|--confirm|--yes]`
- `file recycle-restore --id <cloud-docid> [-y|--confirm|--yes]`
- `file share-record --id <cloud-docid>`
- `file share-create --id <cloud-docid> --name <title> [--is-dir] [--permissions <list>] [--expires-at <time>] [--password <pwd>] [--limit <n>] [-y|--confirm|--yes]`
- `file share-update --share-id <share-id> --id <cloud-docid> --name <title> [--permissions <list>] [-y|--confirm|--yes]`
- `file share-delete --share-id <share-id> [-y|--confirm|--yes]`
- `file share-parse --share-id <share-id|url> [--password <pwd>]`
- `file download-url --id <cloud-docid> [--is-dir] [--name <name>] [--token <share-token>]`
- `file batch-download-url --input <id[:file|dir],...> [--name <zip-name>] [--token <share-token>]`
- `file upload --parent-id <cloud-docid> --path <path> [--name <name>] [--token <share-token>] [-y|--confirm|--yes]`

北航云盘迁移自 `buaa-api` 的 `cloud` 模块，覆盖文档库根目录、个人根目录、目录列表、容量、回收站、分享历史、建议名、目录创建、重命名、移动、复制、删除、回收站删除/恢复、分享创建/更新/删除/解析、单项/批量下载 URL、秒传、小文件上传和 20MiB 分片上传。Core 只通过跨平台抽象 `IHttpClient`、`ICookieStore`、`ICacheStore` 和 `IUploadSource` 访问网络、Cookie、缓存与上传字节源；本地路径读取由 CLI/Platform 完成。

- `file roots` 的 `--root` 可选值为 `all`、`user`、`shared`、`department`、`group`。
- JSON 输出：`data.cloudRoots[]`、`data.cloudRoot`、`data.cloudFiles[]`、`data.cloudSize`、`data.cloudRecycle[]`、`data.cloudShares[]`、`data.cloudShare`、`data.cloudDownload`、写操作 `data.result`。
- `--token <share-token>` 仅作为本次请求的分享授权头使用，不写入 CookieStore、SecureStore、缓存或日志。
- `file upload` 会读取本地文件并可能上传到真实云盘；必须显式 `--parent-id` 和确认参数。错误、日志和测试输出不得泄露本地路径、文件名、token 或 URL query。

## TD 本地与远端确认命令

- `td init [-y|--confirm|--yes]`
- `td image add <path> [--name <name>] [--overwrite] [-y|--confirm|--yes]`
- `td image list`
- `td image delete <name> [--force] [-y|--confirm|--yes]`
- `td user add <student-id> --quick <沙河|学院路> [-y|--confirm|--yes]`
- `td user add <student-id> --entrance <id> --exit <id> --entrance-image <name> --exit-image <name> [-y|--confirm|--yes]`
- `td user list`
- `td user show <student-id>`
- `td user delete <student-id> [-y|--confirm|--yes]`
- `td status`
- `td count [student-id] [--refresh] [-y|--confirm|--yes]`
- `td run --once [-y|--confirm|--yes]`
- `td scheduler once|clear-errors|watch [-y|--confirm|--yes]`

TD 图片命令只管理本地 `td/images` 目录。`td image delete` 会校验安全文件名；图片仍被 TD 用户引用时默认拒绝，只有显式 `--force` 才允许删除。`td count --refresh`、`td run --once` 和 scheduler 远端路径会触发 TD 协议请求或打卡副作用，必须逐次确认，不进入默认 CI 或默认 live smoke。

## 输出与真实性测试边界

`evaluation submit` 的 JSON 输出包含 `accepted`、`message` 和 `result.fields.results`，其中 `results` 是逐课程提交结果数组的 JSON 字符串。LibBook、CGYY、YGDK、Cloud、SRS、SPOC、Signin、Evaluation 当前保持兼容 `FeatureRecord`/`MutationResult` 输出，业务字段位于 `fields`。

所有写操作都必须逐次确认：可显式传入 `--confirm`、`--yes` 或 `-y`；未传时，人类可读模式会询问 `y/N`，`--json` / 脚本模式会 fail-closed 并返回 `InvalidArgument`。默认 live smoke 只执行登录和只读命令；可逆 Cloud 写 smoke 需要显式开关。SRS 选课/退选、BYKC 选退课/签到、课程签到、评教、SPOC 作业提交、WiFi 登录/登出、阳光打卡、场馆预约和图书馆预约不自动真实执行。真实登录和只读请求可能保存 cookie/session；平台 secure store 或 cookie persistence 不可用时必须 fail-closed。
