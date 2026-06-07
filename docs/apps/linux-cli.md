# Linux 命令行应用程序 (ubaa)

> 当前版本阶段：`v0.4.0` 稳定基线加 post-0.4 `buaa-api` 迁移。Linux CLI 与 Windows CLI 共享同一套 Core、命令树、JSON envelope、exit code 和写操作确认门；平台差异只允许出现在 HTTP backend、cookie/session persistence、secure store、路径解析、上传文件读取和 WiFi 网络环境检测等 Platform/CLI 层。

## 构建与运行

```bash
cmake --fresh --preset linux-ninja-debug
cmake --build --preset linux-ninja-debug --target ubaa UBAANextTests UBAANextCliTests
ctest --preset linux-ninja-debug
```

Release CLI：

```bash
cmake --fresh --preset linux-ninja-release
cmake --build --preset linux-ninja-release --target ubaa
```

运行示例：

```bash
./build/linux-ninja-debug/bin/ubaa version --json
./build/linux-ninja-debug/bin/ubaa login --mock 20260000 test --json
./build/linux-ninja-debug/bin/ubaa course today --mock --json
./build/linux-ninja-debug/bin/ubaa live resources --mock --date 2026-06-01 --json
./build/linux-ninja-debug/bin/ubaa live download --mock --course-id mock-course-1 --sub-id mock-sub-1 --out-dir /tmp/ubaanext-live --json
./build/linux-ninja-debug/bin/ubaa file roots --mock --root user --json
./build/linux-ninja-debug/bin/ubaa srs config --mock --json
./build/linux-ninja-debug/bin/ubaa evaluation form --mock --id evaluation-1 --json
```

## 平台能力边界

- Core 不包含 Linux/POSIX、Curl、OpenSSL、Secret Service 或路径策略头；这些能力由 Linux platform adapter 和 CLI 注入。
- Linux 安全存储优先使用 Secret Service/libsecret；不可用时只能走项目明确实现的本地加密 fallback，并以 `secure_store=false` 暴露风险。
- Cookie/session persistence、HTTP/TLS/redirect/proxy、上传文件读取和路径解析由 Platform/CLI 层负责。
- `INetworkEnvironment` 在 Linux 平台负责本机 IPv4 与校园网连通性检测；未知平台 fail-closed。
- 写操作默认仍需 `--confirm`、`--yes`、`-y` 或人类可读交互输入 `y`；`--json` / 脚本模式缺少确认时必须返回 `InvalidArgument`。

## buaa-api 迁移命令

Linux CLI 与 Windows CLI 使用相同命令合同，完整列表见 [CLI 命令 API](../api/cli-command-api.md)。

- AAS/App：`term/week/course/exam/grade/classroom`
- Live：`live week --start-date <yyyy-MM-dd> --end-date <yyyy-MM-dd>`、`live resources/detail/download`
- SPOC：`spoc week/schedule/courses/assignments/assignment show/homework submit`
- Signin：`signin today/schedule/courses/course schedule/do`
- Cloud：`file roots/root/list/size/recycle/shares/suggest-name/mkdir/rename/move/copy/delete/recycle-delete/recycle-restore/share-* /download-url/batch-download-url/upload`
- SRS：`srs config/batch/course query/preselected/selected/course preselect/select/drop`
- Evaluation：`evaluation list/form/submit/form submit`
- WiFi：`wifi login/logout`

## 北航云盘

Cloud 真实上传入口：

```bash
ubaa file upload --parent-id <cloud-docid> --path <path> [--name <name>] [--token <share-token>] -y --json
```

Cloud Core 使用 `IUploadSource` 消费上传字节源，Linux CLI 负责读取本地文件。上传支持秒传、小文件上传和 20MiB 分片上传；错误和日志不得泄露本地路径、文件名、token 或 URL query。Cloud 可逆写 smoke 只在用户显式开启时运行。

## 课堂资源下载

Linux CLI 与 Windows CLI 的课堂资源命令完全一致：

```bash
ubaa live resources --date <yyyy-MM-dd> [--from-course] [--status live|playback|generating|all] --json
ubaa live detail --course-id <course-id> --sub-id <sub-id> [--date <yyyy-MM-dd>] --json
ubaa live download --course-id <course-id> --sub-id <sub-id> --out-dir <dir> [--include ppt,video] [--overwrite] --json
```

PPT 下载会生成 `.pptx`。视频为 MP4 直链时直接落盘；视频为 HLS/m3u8 时会尝试调用系统 `ffmpeg`，未安装或合并失败时写入 `.m3u8.url` sidecar 并返回 `status:"partial"`。默认测试只覆盖 mock 下载和 PPTX ZIP 结构；真实下载必须通过 live smoke 显式开启。

## 真实性测试边界

默认 live smoke 只执行登录和只读命令。课堂资源真实下载需要额外设置 `UBAANEXT_LIVE_DOWNLOAD=1`，并建议先用 `UBAANEXT_LIVE_DOWNLOAD_INCLUDE=ppt` 做小范围验证。不会自动执行：SRS 选课/退选、BYKC 选课/退选/签到、课程签到、评教提交、SPOC 作业提交、WiFi 登录/登出、阳光打卡、场馆预约/取消、图书馆座位预约/取消。

Linux CLI 的新增命令必须同时更新：

- [CLI 命令 API](../api/cli-command-api.md)
- [Windows CLI](windows-cli.md)
- [buaa-api 迁移矩阵](../reports/buaa-api-migration-matrix.md)
- CLI integration/golden tests
