#include "CommandHandlers.hpp"

#include "Console.hpp"
#include "OutputFormatter.hpp"

#include <UBAANext/Version.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace UBAANextCli {
namespace {

using json = nlohmann::json;

void add_command(json &commands, const std::string &name, const std::string &description, const json &options = {}) {
    json command = { {"name", name}, {"description", description} };
    if (!options.empty()) {
        command["options"] = options.is_array() ? options : json::array({options});
    }
    commands.push_back(command);
}

json option(const std::string &name,
            const std::string &description,
            bool required,
            const std::string &placeholder = {},
            const std::string &source_command = {},
            const std::string &source_field = {},
            const std::string &example = {},
            const std::string &note = {}) {
    json value = {{"name", name}, {"description", description}, {"required", required}};
    if (!placeholder.empty()) value["placeholder"] = placeholder;
    if (!source_command.empty()) value["sourceCommand"] = source_command;
    if (!source_field.empty()) value["sourceField"] = source_field;
    if (!example.empty()) value["example"] = example;
    if (!note.empty()) value["note"] = note;
    return value;
}

} // namespace

nlohmann::json get_help_json() {
    json commands = json::array();

    add_command(commands, "version", "显示版本信息");
    add_command(commands, "help", "显示帮助信息；使用 help --json 查看机器可读命令目录");

    json login_options = {
        option("account", "账号/学号位置参数；登录成功后默认保存", true, "account", {}, {}, "ubaa login <账号> <密码>"),
        option("password", "密码位置参数；不得出现在日志、错误或测试输出中", true, "password"),
        option("--username", "兼容旧写法：学号", false, "account"),
        option("--password", "兼容旧写法：密码", false, "password"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    json relogin_options = {
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct；不传时复用 login 保存的连接模式", false, "vpn|direct"),
        option("--confirm", "确认清理旧本地会话并复用已保存账号密码；也可用 --yes 或 -y", false),
    };
    add_command(commands, "login", "登录并默认保存账号密码", login_options);
    add_command(commands, "relogin", "复用 login 保存的账号密码重新登录并替换本地会话", relogin_options);

    add_command(commands, "mode", "显示当前连接模式");
    add_command(commands, "mode vpn", "切换为 VPN 模式");
    add_command(commands, "mode direct", "切换为直连模式");

    add_command(commands, "whoami", "显示当前用户信息");

    json course_options = {
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "course today", "显示今天的课程", course_options);

    json course_date_options = {
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "course date", "显示指定日期的课程", course_date_options);

    json course_week_options = {
        option("--week", "教学周次，范围 1-30", true, "n"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "course week", "显示指定周次的课程", course_week_options);

    json exam_options = {
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "exam list", "显示考试列表", exam_options);

    json classroom_options = {
        option("--campus", "校区 ID，范围 1-10", true, "campus-id"),
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "classroom query", "查询空闲教室", classroom_options);

    json term_options = {
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    };
    add_command(commands, "term list", "显示学期列表", term_options);
    add_command(commands, "week list", "显示教学周列表", term_options);
    add_command(commands, "live week", "显示课堂直播周课表", json{
        option("--start-date", "周起始日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
        option("--end-date", "周结束日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file roots", "显示北航云盘文档库根目录；root-id 来自输出记录 id 字段", json{
        option("--root", "根目录类型过滤: all|user|shared|department|group；默认 all", false, "all|user|shared|department|group"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file root", "显示北航云盘个人文档库根目录；docid 来自输出记录 id 字段", json{
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file list", "列出北航云盘目录内容；docid 来自 file roots 或 file root 输出记录 id 字段", json{
        option("--id", "云盘目录 docid", true, "docid", "file roots 或 file root", "id"),
        option("--token", "可选分享链接访问 token，仅作为 x-as-authorization 使用，不会写入本地存储", false, "share-token"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file size", "显示北航云盘条目容量摘要；docid 来自 file list 或 file root 输出记录 id 字段", json{
        option("--id", "云盘条目 docid", true, "docid", "file list 或 file root", "id"),
        option("--token", "可选分享链接访问 token，仅作为 x-as-authorization 使用，不会写入本地存储", false, "share-token"),
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file recycle", "列出北航云盘回收站；删除和恢复需使用显式写命令", json{
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file shares", "列出北航云盘分享历史；创建、修改、删除需使用显式写命令", json{
#if UBAANEXT_ENABLE_MOCKS
        option("--mock", "使用模拟数据", false),
#endif
        option("--mode", "连接模式: vpn|direct", false, "vpn|direct"),
    });
    add_command(commands, "file suggest-name", "获取指定目录下冲突时的建议名称", json{
        option("--parent-id", "父目录 docid，来自 file root/list 输出记录 id 字段", true, "docid", "file root 或 file list", "id"),
        option("--name", "期望名称", true, "name"),
    });
    add_command(commands, "file mkdir", "创建北航云盘目录", json{
        option("--parent-id", "父目录 docid，来自 file root/list 输出记录 id 字段", true, "docid", "file root 或 file list", "id"),
        option("--name", "目录名", true, "name"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file rename", "重命名北航云盘文件或目录", json{
        option("--id", "条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
        option("--name", "新名称", true, "name"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file move", "移动北航云盘文件或目录", json{
        option("--id", "源条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
        option("--dest-id", "目标父目录 docid，来自 file list/root 输出记录 id 字段", true, "docid", "file list 或 file root", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file copy", "复制北航云盘文件或目录；带 --token 时可用于分享另存为", json{
        option("--id", "源条目 docid，来自 file list 或 file share-parse 输出记录 id 字段", true, "docid", "file list 或 file share-parse", "id"),
        option("--dest-id", "目标父目录 docid，来自 file list/root 输出记录 id 字段", true, "docid", "file list 或 file root", "id"),
        option("--token", "分享访问 token，仅作为 x-as-authorization 使用", false, "share-token"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file delete", "删除北航云盘条目到回收站", json{
        option("--id", "条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file recycle-delete", "彻底删除回收站条目", json{
        option("--id", "回收站条目 docid，来自 file recycle 输出记录 id 字段", true, "docid", "file recycle", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file recycle-restore", "恢复回收站条目", json{
        option("--id", "回收站条目 docid，来自 file recycle 输出记录 id 字段", true, "docid", "file recycle", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file share-record", "显示单个云盘条目的分享记录", json{
        option("--id", "条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
    });
    add_command(commands, "file share-create", "创建公开分享链接", json{
        option("--id", "条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
        option("--name", "分享标题", true, "title"),
        option("--is-dir", "条目是目录时传入", false),
        option("--permissions", "权限列表: display,preview,download,create,modify,upload", false, "permissions"),
        option("--expires-at", "过期时间；默认 1970-01-01T08:00:00+08:00 表示不过期", false, "datetime"),
        option("--limited-times", "访问次数限制，-1 表示不限", false, "n"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file share-update", "更新公开分享链接", json{
        option("--share-id", "分享 ID，来自 file shares 或 file share-record 输出记录 id 字段", true, "share-id", "file shares", "id"),
        option("--id", "条目 docid，来自 file list 输出记录 id 字段", true, "docid", "file list", "id"),
        option("--name", "分享标题", true, "title"),
        option("--permissions", "权限列表: display,preview,download,create,modify,upload", false, "permissions"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file share-delete", "删除公开分享链接", json{
        option("--share-id", "分享 ID，来自 file shares 或 file share-record 输出记录 id 字段", true, "share-id", "file shares", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "file share-parse", "解析公开分享链接为可继续 list/copy/download 的条目", json{
        option("--id", "分享 ID 或 https://bhpan.buaa.edu.cn/link/<id>", true, "share-id|url"),
        option("--password", "分享密码；也可用 --share-password", false, "password"),
    });
    add_command(commands, "file download-url", "获取单个文件下载 URL；目录需加 --is-dir 以打包下载", json{
        option("--id", "条目 docid，来自 file list 或 file share-parse 输出记录 id 字段", true, "docid", "file list 或 file share-parse", "id"),
        option("--name", "打包下载文件名；单文件时可省略", false, "name"),
        option("--is-dir", "条目是目录时传入", false),
        option("--token", "分享访问 token，仅作为 x-as-authorization 使用", false, "share-token"),
    });
    add_command(commands, "file batch-download-url", "获取多个文件/目录的打包下载 URL", json{
        option("--input", "逗号分隔 id[:file|dir] 列表；例如 a:file,b:dir", true, "items"),
        option("--name", "zip 文件名", false, "name"),
        option("--token", "分享访问 token，仅作为 x-as-authorization 使用", false, "share-token"),
    });
    add_command(commands, "file upload", "上传本地文件到北航云盘，支持秒传、小文件 PUT 和 20MiB 分片上传", json{
        option("--parent-id", "目标父目录 docid，来自 file root/list 输出记录 id 字段", true, "docid", "file root 或 file list", "id"),
        option("--path", "待上传文件路径；CLI 读取本地文件，Core 只消费上传流", true, "path"),
        option("--name", "云盘内文件名；省略时使用本地文件名", false, "name"),
        option("--token", "分享目录上传 token，仅作为 x-as-authorization 使用", false, "share-token"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "srs config", "显示选课系统配置和选课批次");
    add_command(commands, "srs batch", "从选课页面解析当前预选批次 ID");
    add_command(commands, "srs course query", "查询选课课程", json{
        option("--scope", "课程范围: TJKC/FANKC/FAWKC/CXKC/YYKC/TYKC/XGKC/KYKT/ALLKC，也支持英文别名", false, "scope"),
        option("--page", "页码", false, "n"),
        option("--size", "每页数量", false, "n"),
        option("--campus", "校区: 1 学院路，2 沙河", false, "1|2"),
        option("--all", "显示冲突课程；默认隐藏冲突课程", false),
        option("--requirement", "课程性质代码，例如 01/02/03/04", false, "code"),
        option("--category", "课程类型代码，例如 A/FG/011/031", false, "code"),
        option("--keyword", "关键词", false, "text"),
    });
    add_command(commands, "srs preselected", "查询预选结果");
    add_command(commands, "srs selected", "查询已选课程");
    add_command(commands, "srs course preselect", "预选课程", json{
        option("--id", "教学班 JXBID，来自 srs course query 输出 id 字段", true, "clazzId", "srs course query", "id"),
        option("--scope", "课程范围，来自 srs course query 输出 fields.scope", true, "scope", "srs course query", "fields.scope"),
        option("--token", "secretVal，来自 srs course query 输出 fields.secretVal", true, "secretVal", "srs course query", "fields.secretVal"),
        option("--batch-id", "批次 ID，来自 srs batch 或 srs config 输出 id 字段", true, "batchId", "srs batch", "id"),
        option("--index", "志愿序号", true, "n"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "srs course select", "正选课程", json{
        option("--id", "教学班 JXBID，来自 srs course query 输出 id 字段", true, "clazzId", "srs course query", "id"),
        option("--scope", "课程范围，来自 srs course query 输出 fields.scope", true, "scope", "srs course query", "fields.scope"),
        option("--token", "secretVal，来自 srs course query 输出 fields.secretVal", true, "secretVal", "srs course query", "fields.secretVal"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "srs course drop", "退选课程", json{
        option("--id", "教学班 JXBID，来自 srs selected 输出 id 字段", true, "clazzId", "srs selected", "id"),
        option("--scope", "课程范围，来自 srs selected 输出 fields.scope", true, "scope", "srs selected", "fields.scope"),
        option("--token", "secretVal，来自 srs selected 输出 fields.secretVal", true, "secretVal", "srs selected", "fields.secretVal"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });

    add_command(commands, "config show", "显示当前配置");
    add_command(commands, "capability show", "显示当前平台能力声明；capability 不代表真实登录、真实写 UI 或业务 API 完成");

    json config_set_options = {
        option("--key", "配置键", true, "mode|proxy|cache"),
        option("--value", "配置值", true, "value"),
        option("--confirm", "确认修改本地配置；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    };
    add_command(commands, "config set", "设置配置项", config_set_options);

    json confirm_options = {
        option("--confirm", "确认执行有副作用操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    };
    add_command(commands, "cache clear", "清除缓存", confirm_options);
    add_command(commands, "logout", "登出并清除本地会话", confirm_options);

    json td_local_write_options = {
        option("--confirm", "确认本地 TD 配置写操作；也可用 --yes 或 -y", true),
    };
    add_command(commands, "td init", "初始化 TD 本地配置、用户、状态、图片和日志目录", td_local_write_options);
    add_command(commands, "td image add", "添加 TD 图片到本地 TD 图片目录", json{
        option("path", "图片源文件路径位置参数；也可用 --path", true, "path"),
        option("--path", "图片源文件路径", false, "path"),
        option("--name", "保存到 TD 图片目录中的文件名", false, "name"),
        option("--overwrite", "允许覆盖同名本地 TD 图片", false),
        option("--confirm", "确认本地图片写入；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td image list", "列出本地 TD 图片");
    add_command(commands, "td user add", "添加 TD 用户；支持 --quick 沙河/学院路 或显式机器与图片参数", json{
        option("student-id", "学号位置参数；也可用 --student-id", true, "student-id"),
        option("--student-id", "学号", false, "student-id"),
        option("--quick", "按校区从默认机器池和已有图片快速生成用户，仅支持 沙河 或 学院路", false, "沙河|学院路"),
        option("--card-id", "可选卡号；未指定时按模型规则从学号派生", false, "card-id"),
        option("--entrance", "入口机器 ID；显式模式必填", false, "machine-id"),
        option("--exit", "出口机器 ID；显式模式必填", false, "machine-id"),
        option("--entrance-image", "入口图片文件名；显式模式必填，来自 td image list", false, "image-name", "td image list", "id"),
        option("--exit-image", "出口图片文件名；显式模式必填，来自 td image list", false, "image-name", "td image list", "id"),
        option("--rounds", "目标往返次数，默认使用 TD 模型默认值", false, "n"),
        option("--wait-min", "每轮等待下限分钟数", false, "minutes"),
        option("--wait-max", "每轮等待上限分钟数", false, "minutes"),
        option("--overwrite", "允许更新已存在的 TD 用户", false),
        option("--confirm", "确认本地用户写入；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td user list", "列出本地 TD 用户");
    add_command(commands, "td user show", "显示本地 TD 用户详情", json{
        option("student-id", "学号位置参数；也可用 --student-id", true, "student-id", "td user list", "id"),
        option("--student-id", "学号", false, "student-id"),
    });
    add_command(commands, "td user delete", "删除本地 TD 用户", json{
        option("student-id", "学号位置参数；也可用 --student-id", true, "student-id", "td user list", "id"),
        option("--student-id", "学号", false, "student-id"),
        option("--confirm", "确认本地用户删除；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td status", "显示本地 TD 运行状态缓存");
    add_command(commands, "td count", "读取本地缓存的 TD 锻炼次数；--refresh 会在确认后通过 TD 服务器刷新", json{
        option("student-id", "可选学号；未指定时显示所有本地 TD 用户", false, "student-id", "td user list", "id"),
        option("--refresh", "通过 TD check 协议刷新服务器次数；具有写语义，必须显式确认", false),
        option("--confirm", "与 --refresh 配合确认 write-like 协议请求；也可用 --yes 或 -y", false),
    });
    add_command(commands, "td run", "执行一次 TD 编排；确认后通过 TD 服务器完成入口/出口请求和图片上传", json{
        option("--once", "执行一次 TD 编排", true),
        option("--confirm", "确认真实写语义；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td scheduler once", "执行一次 TD 调度 tick；按时间窗口推进入口/出口状态并写入日志", json{
        option("--confirm", "确认调度 tick 可能触发 TD check/upload 请求；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td scheduler clear-errors", "清理今日 TD error 状态，使调度器可重新从入口开始", json{
        option("--date", "可选日期，格式 yyyy-MM-dd；默认今天", false, "yyyy-MM-dd"),
        option("--confirm", "确认本地状态写入；也可用 --yes 或 -y", true),
    });
    add_command(commands, "td scheduler watch", "后台持续轮询 TD 调度 tick；人类可读模式下按 Ctrl-C 停止", json{
        option("--poll-seconds", "轮询间隔秒数；默认使用 TD config.poll_seconds", false, "seconds"),
        option("--confirm", "确认持续调度可能触发 TD check/upload 请求；也可用 --yes 或 -y", true),
    });

    add_command(commands, "user info", "显示用户信息");
    add_command(commands, "app version", "显示应用版本信息");
    add_command(commands, "app announcement", "显示公告");
    json grade_list_options = {
        option("--term", "学期代码", false, "term-code"),
        option("--all", "显示全部成绩", false),
    };
    add_command(commands, "grade list", "显示指定学期成绩", grade_list_options);
    add_command(commands, "grade all", "显示全部成绩");
    add_command(commands, "spoc week", "显示 SPOC 当前教学周和当前学期");
    add_command(commands, "spoc schedule", "显示 SPOC 指定日期范围课表", json{
        option("--start-date", "周起始日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
        option("--end-date", "周结束日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
    });
    add_command(commands, "spoc courses", "显示 SPOC 学期课程；课程 ID 来自输出记录的 id 字段", json{
        option("--term", "学期代码，通常来自 spoc week 输出记录的 fields.term", true, "term-code", "spoc week", "fields.term"),
    });
    add_command(commands, "spoc assignments", "显示 SPOC 作业；详情 ID 来自输出记录的 id 字段", json{
        option("--pending-only", "只显示待处理作业", false),
        option("--include-expired", "包含已过期作业", false),
    });
    add_command(commands, "spoc assignment show", "显示 SPOC 作业详情", json{
        option("--id", "SPOC 作业 ID，来自 spoc assignments 输出记录的 id 字段", true, "assignment-id", "spoc assignments", "id", "spoc assignment show --id <assignment-id>"),
    });
    add_command(commands, "spoc homework submit", "提交 SPOC 作业；真实写操作必须确认", json{
        option("--id", "SPOC 作业 ID，来自 spoc assignments 输出记录的 id 字段", true, "assignment-id", "spoc assignments", "id"),
        option("--course-id", "课程 ID，来自 spoc assignments 输出记录的 fields.courseId 字段", true, "course-id", "spoc assignments", "fields.courseId"),
        option("--file-id", "已上传文件 ID；来自后续 spoc upload 输出记录的 id 字段", true, "file-id"),
        option("--name", "已上传文件名", true, "file-name"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y", true),
    });
    add_command(commands, "judge assignments", "显示希冀作业；详情 ID 来自输出记录的 id 字段", json{
        option("--course-id", "可选课程过滤 ID，通常来自 judge assignments 输出记录的 fields.courseId 字段", false, "course-id", "judge assignments", "fields.courseId"),
        option("--include-expired", "包含已过期作业", false),
        option("--include-history", "包含历史作业", false),
    });
    add_command(commands, "judge assignment show", "显示希冀作业概要", json{
        option("--assignment-id", "希冀作业 ID，来自 judge assignments 输出记录的 id 字段", true, "assignment-id", "judge assignments", "id"),
    });
    add_command(commands, "judge assignment details", "显示希冀作业详情", json{
        option("--assignment-id", "希冀作业 ID，来自 judge assignments 输出记录的 id 字段", true, "assignment-id", "judge assignments", "id"),
    });
    add_command(commands, "judge assignment details-batch", "批量显示希冀作业详情", json{
        option("--input", "JSON、@file 或逗号分隔的希冀作业 ID；ID 来自 judge assignments 输出记录的 id 字段", true, "json|@file|ids", "judge assignments", "id"),
    });
    add_command(commands, "signin today", "显示今日课程签到；签到 ID 来自输出记录的 id 字段");
    add_command(commands, "signin schedule", "显示指定日期课程签到；签到 ID 来自输出记录的 id 字段", json{
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
    });
    add_command(commands, "signin courses", "显示指定学期的签到课程；课程 ID 来自输出记录的 id 字段", json{
        option("--term", "学期代码", true, "term-code"),
    });
    add_command(commands, "signin course schedule", "显示单门课程的签到明细；签到 ID 来自输出记录的 id 字段", json{
        option("--course-id", "课程 ID，来自 signin courses 输出记录的 id 字段", true, "course-id", "signin courses", "id"),
    });
    add_command(commands, "signin do", "执行课程签到", json{
        option("--id", "签到任务 ID，来自 signin today 输出记录的 id 字段", false, "signin-id", "signin today", "id"),
        option("--course-id", "兼容课程 ID 写法，来自 signin today 输出记录的 id 字段", false, "course-id", "signin today", "id"),
        option("--confirm", "确认执行；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "wifi login", "登录 BUAA-WiFi/BUAA-Mobile 网关；必须在校园网环境并显式确认", json{
        option("--username", "网关账号；未提供时使用保存的 login 账号", false, "account"),
        option("--password", "网关密码；未提供时使用保存的 login 密码", false, "password"),
        option("--confirm", "确认真实网关写操作；也可用 --yes 或 -y", true),
    });
    add_command(commands, "wifi logout", "登出 BUAA-WiFi/BUAA-Mobile 网关；必须在校园网环境并显式确认", json{
        option("--username", "网关账号；未提供时使用保存的 login 账号", false, "account"),
        option("--confirm", "确认真实网关写操作；也可用 --yes 或 -y", true),
    });
    add_command(commands, "bykc profile", "显示博雅资料");
    add_command(commands, "bykc courses", "显示博雅课程；课程 ID 来自输出记录的 id 字段", json{
        option("--page", "页码", false, "n"),
        option("--size", "每页数量", false, "n"),
        option("--all", "显示全部", false),
        option("--status", "课程状态过滤", false, "status"),
        option("--category", "课程分类过滤", false, "name"),
        option("--sub-category", "课程子分类过滤", false, "name"),
        option("--campus", "校区 ID", false, "campus-id"),
        option("--keyword", "关键词", false, "text"),
    });
    add_command(commands, "bykc chosen", "显示已选博雅课程；课程 ID 来自输出记录的 id 字段或 fields.courseId 字段");
    add_command(commands, "bykc stats", "显示博雅统计");
    add_command(commands, "bykc course show", "显示博雅课程详情", json{
        option("--course-id", "博雅课程 ID，来自 bykc courses 或 bykc chosen 输出记录的 id 字段", true, "course-id", "bykc courses 或 bykc chosen", "id"),
    });
    add_command(commands, "bykc select", "选择博雅课程", json{
        option("--course-id", "博雅课程 ID，来自 bykc courses 输出记录的 id 字段", true, "course-id", "bykc courses", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "bykc unselect", "退选博雅课程", json{
        option("--course-id", "博雅课程 ID，来自 bykc chosen 输出记录的 id 字段", true, "course-id", "bykc chosen", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "bykc sign", "博雅签到/签退", json{
        option("--course-id", "博雅课程 ID，来自 bykc chosen 输出记录的 id 字段", true, "course-id", "bykc chosen", "id"),
        option("--sign-type", "签到类型：1 表示签到，2 表示签退", true, "1|2"),
        option("--lat", "真实纬度；必须显式提供，不会默认伪造位置", true, "latitude"),
        option("--lng", "真实经度；必须显式提供，不会默认伪造位置", true, "longitude"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "cgyy sites", "显示场馆列表；场馆 ID 来自输出记录的 id 字段");
    add_command(commands, "cgyy purpose-types", "显示场馆预约用途类型；用途类型 ID 来自输出记录的 id 字段");
    add_command(commands, "cgyy day-info", "显示场馆日期可预约信息", json{
        option("--site-id", "场馆 ID，来自 cgyy sites 输出记录的 id 字段", true, "site-id", "cgyy sites", "id"),
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
    });
    add_command(commands, "cgyy orders", "显示场馆预约订单；订单 ID 来自输出记录的 id 字段", json{
        option("--page", "页码", false, "n"),
        option("--size", "每页数量", false, "n"),
    });
    add_command(commands, "cgyy order show", "显示场馆预约订单详情", json{
        option("--order-id", "场馆订单 ID，来自 cgyy orders 输出记录的 id 字段", true, "order-id", "cgyy orders", "id"),
    });
    add_command(commands, "cgyy order lock-code", "显示场馆预约锁码", json{
        option("--order-id", "场馆订单 ID；mock 兼容参数，真实模式通常读取当前锁码", false, "order-id", "cgyy orders", "id"),
    });
    add_command(commands, "cgyy reserve", "预约场馆", json{
        option("--site-id", "场馆 ID，来自 cgyy sites 输出记录的 id 字段", true, "site-id", "cgyy sites", "id"),
        option("--space-id", "场地 ID，来自 cgyy day-info 输出记录的 id 字段", true, "space-id", "cgyy day-info", "id"),
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
        option("--id", "预约时段 ID，来自 cgyy day-info 输出记录的 fields.timeId 字段", true, "time-id", "cgyy day-info", "fields.timeId"),
        option("--purpose-type", "用途类型 ID，来自 cgyy purpose-types 输出记录的 id 字段", true, "purpose-type-id", "cgyy purpose-types", "id"),
        option("--theme", "预约主题", true, "theme"),
        option("--phone", "联系电话", true, "phone"),
        option("--joiners", "参与人列表", true, "joiners"),
        option("--captcha", "验证码，必须由用户从真实页面获得，CLI 不绕过验证码", true, "captcha"),
        option("--token", "预约上下文 token，来自 cgyy day-info 输出记录的 fields.token 字段", true, "token", "cgyy day-info", "fields.token"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "cgyy order cancel", "取消场馆预约", json{
        option("--order-id", "场馆订单 ID，来自 cgyy orders 输出记录的 id 字段", true, "order-id", "cgyy orders", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "libbook libraries", "显示图书馆列表；图书馆 ID 来自输出记录的 id 字段");
    add_command(commands, "libbook areas", "显示图书馆区域列表；区域 ID 来自输出记录的 id 字段", json{
        option("--library-id", "图书馆 ID，来自 libbook libraries 输出记录的 id 字段", true, "library-id", "libbook libraries", "id"),
        option("--date", "日期，格式 yyyy-MM-dd；未指定时使用默认日期", false, "yyyy-MM-dd"),
        option("--storey-id", "楼层 ID，来自 libbook areas 输出记录的 fields.storeyId 字段", false, "storey-id", "libbook areas", "fields.storeyId"),
    });
    add_command(commands, "libbook seats", "显示座位列表；座位 ID 来自输出记录的 id 字段", json{
        option("--area-id", "区域 ID，来自 libbook areas 输出记录的 id 字段", true, "area-id", "libbook areas", "id"),
        option("--date", "日期，格式 yyyy-MM-dd；未指定时使用默认日期", false, "yyyy-MM-dd"),
        option("--start-time", "开始时间，格式 HH:mm", false, "HH:mm"),
        option("--end-time", "结束时间，格式 HH:mm", false, "HH:mm"),
    });
    add_command(commands, "libbook reservations", "显示图书馆座位预约记录；预约 ID 来自输出记录的 id 字段", json{
        option("--page", "页码", false, "n"),
        option("--size", "每页数量", false, "n"),
    });
    add_command(commands, "libbook area show", "显示图书馆区域详情", json{
        option("--area-id", "区域 ID，来自 libbook areas 输出记录的 id 字段", true, "area-id", "libbook areas", "id"),
    });
    add_command(commands, "libbook book", "预约图书馆座位", json{
        option("--seat-id", "座位 ID，来自 libbook seats 输出记录的 id 字段", true, "seat-id", "libbook seats", "id"),
        option("--date", "日期，格式 yyyy-MM-dd", true, "yyyy-MM-dd"),
        option("--segment", "预约时段；也可改用 --start-time/--end-time", false, "segment"),
        option("--start-time", "开始时间，格式 HH:mm；与 --end-time 配套使用", false, "HH:mm"),
        option("--end-time", "结束时间，格式 HH:mm；与 --start-time 配套使用", false, "HH:mm"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "libbook cancel", "取消图书馆座位预约", json{
        option("--booking-id", "图书馆预约 ID，来自 libbook reservations 输出记录的 id 字段", true, "booking-id", "libbook reservations", "id"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "ygdk overview", "显示阳光打卡概览；打卡项目 ID 来自 status=item 的输出记录 id 字段");
    add_command(commands, "ygdk records", "显示阳光打卡记录", json{
        option("--page", "页码", false, "n"),
        option("--size", "每页数量", false, "n"),
    });
    add_command(commands, "ygdk submit", "提交阳光打卡", json{
        option("--id", "打卡项目 ID，来自 ygdk overview 输出中 status=item 的记录 id 字段；可省略让服务选择默认项目", false, "item-id", "ygdk overview", "id"),
        option("--item-id", "打卡项目 ID，来自 ygdk overview 输出中 status=item 的记录 id 字段", false, "item-id", "ygdk overview", "id"),
        option("--start-time", "开始时间", true, "time"),
        option("--end-time", "结束时间", true, "time"),
        option("--place", "打卡地点文本", true, "place"),
        option("--photo", "照片路径，本地文件路径会被读取并上传", true, "path"),
        option("--share", "同步共享", false),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "evaluation list", "显示评教任务；评教任务 ID 来自输出记录的 id 字段");
    add_command(commands, "evaluation form", "显示指定评教表单摘要；题目数和表单上下文字段用于提交前确认", json{
        option("--id", "评教任务 ID，来自 evaluation list 输出记录的 id 字段", false, "evaluation-id", "evaluation list", "id"),
        option("--course-id", "兼容课程代码写法，来自 evaluation list 输出记录 fields.courseCode/kcdm", false, "course-code", "evaluation list", "fields.courseCode"),
    });
    add_command(commands, "evaluation submit", "提交评教", json{
        option("--id", "评教任务 ID，来自 evaluation list 输出记录的 id 字段；省略时提交可处理的评教任务集合", false, "evaluation-id", "evaluation list", "id"),
        option("--course-id", "兼容课程代码写法，来自 evaluation list 输出记录 fields.courseCode/kcdm", false, "course-code", "evaluation list", "fields.courseCode"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "evaluation form submit", "提交指定评教表单；默认按 buaa-api 填充策略生成答案", json{
        option("--id", "评教任务 ID，来自 evaluation list 输出记录的 id 字段", false, "evaluation-id", "evaluation list", "id"),
        option("--course-id", "兼容课程代码写法，来自 evaluation list 输出记录 fields.courseCode/kcdm", false, "course-code", "evaluation list", "fields.courseCode"),
        option("--reason", "满分或不及格提交时的 10-200 字原因；默认分数通常不需要", false, "text"),
        option("--confirm", "确认写操作；也可用 --yes 或 -y，缺少时人类可读模式会询问 y/N", true),
    });
    add_command(commands, "todo list", "显示待办聚合", json{
        option("--pending-only", "只显示待处理项目", false),
        option("--all", "包含非待处理项目", false),
    });
    return {{"ok", true}, {"data", {{"commands", commands}, {"version", UBAANEXT_VERSION_STRING}}}, {"error", nullptr}};
}

void print_usage() {
    Console::println("用法:");
    Console::println("  ubaa <command> [options]\n");
    Console::println("阅读提示:");
    Console::println("  <...> 表示需要替换的值，例如 <assignment-id> 不是字面量。");
    Console::println("  资源 ID 通常来自对应列表命令输出记录的 id 字段；下方会标出来源。");
    Console::println("  写命令默认具备平台写能力，但每次仍必须确认：可传 --confirm、--yes 或 -y。");
    Console::println("  未传确认参数时，人类可读模式会询问 y/N；--json/脚本模式应显式传确认参数以避免交互。\n");

    Console::println("基础命令");
    Console::println("  version                                  显示版本");
    Console::println("  help                                     显示帮助；help --json 输出机器可读命令目录");
    Console::println("  login <账号> <密码>                     登录并默认保存账号密码（优先安全/加密存储，必要时允许明文 fallback）");
    Console::println("  relogin [-y|--confirm|--yes]           复用 login 保存的账号密码重新登录并替换本地会话");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  login --mock <账号> <密码>              模拟登录并默认保存账号密码");
    Console::println("  relogin --mock [-y|--confirm|--yes]    模拟复用已保存账号密码重新登录");
#endif
    Console::println("  mode                                     显示当前连接模式");
    Console::println("  mode direct                              切换为直连模式");
    Console::println("  mode vpn                                 切换为 VPN 模式");
    Console::println("  capability show                          显示当前平台能力声明；不代表真实登录或真实写 UI 已完成");
    Console::println("  whoami                                   显示当前用户");
    Console::println("  logout [-y|--confirm|--yes]                         登出并清除本地会话");

    Console::println("\n教学、教务和成绩");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  course today [--mock]                    显示今天的课程");
#else
    Console::println("  course today                             显示今天的课程");
#endif
    Console::println("  course date --date <yyyy-MM-dd>          显示指定日期课程");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  course week [--mock] --week <n>          显示指定周次课程");
#else
    Console::println("  course week --week <n>                   显示指定周次课程");
#endif
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  exam list [--mock]                       显示考试");
#else
    Console::println("  exam list                                显示考试");
#endif
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  classroom query [--mock] --campus <campus-id> --date <yyyy-MM-dd>");
#else
    Console::println("  classroom query --campus <campus-id> --date <yyyy-MM-dd>");
#endif
    Console::println("                                           查询空闲教室；campus-id 是 1-10 的校区编号");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  term list [--mock]                       显示学期列表");
    Console::println("  week list [--mock]                       显示教学周列表");
#else
    Console::println("  term list                                显示学期列表");
    Console::println("  week list                                显示教学周列表");
#endif
    Console::println("  user info                                显示用户信息");
    Console::println("  live week --start-date <yyyy-MM-dd> --end-date <yyyy-MM-dd>");
    Console::println("                                           显示课堂直播周课表；通常使用教学周周一到周日日期");
    Console::println("  file roots [--root all|user|shared|department|group]");
    Console::println("                                           显示北航云盘文档库根目录；docid 在输出记录的 id 字段");
    Console::println("  file root                                显示北航云盘个人文档库根目录");
    Console::println("  file list --id <docid> [--token <share-token>]");
    Console::println("                                           列出北航云盘目录内容；docid 来自 file roots 或 file root");
    Console::println("  file size --id <docid> [--token <share-token>]");
    Console::println("                                           显示北航云盘条目容量摘要；token 仅用于分享链接只读访问");
    Console::println("  file recycle                             列出北航云盘回收站");
    Console::println("  file shares                              列出北航云盘分享历史");
    Console::println("  file suggest-name --parent-id <docid> --name <name>");
    Console::println("                                           获取冲突时的建议名称");
    Console::println("  file mkdir --parent-id <docid> --name <name> [-y|--confirm|--yes]");
    Console::println("  file rename --id <docid> --name <name> [-y|--confirm|--yes]");
    Console::println("  file move --id <docid> --dest-id <parent-docid> [-y|--confirm|--yes]");
    Console::println("  file copy --id <docid> --dest-id <parent-docid> [--token <share-token>] [-y|--confirm|--yes]");
    Console::println("  file delete --id <docid> [-y|--confirm|--yes]");
    Console::println("  file recycle-delete --id <docid> [-y|--confirm|--yes]");
    Console::println("  file recycle-restore --id <docid> [-y|--confirm|--yes]");
    Console::println("  file share-record --id <docid>           显示单个条目的分享记录");
    Console::println("  file share-create --id <docid> --name <title> [--is-dir] [--permissions <list>] [-y|--confirm|--yes]");
    Console::println("  file share-update --share-id <id> --id <docid> --name <title> [--permissions <list>] [-y|--confirm|--yes]");
    Console::println("  file share-delete --share-id <id> [-y|--confirm|--yes]");
    Console::println("  file share-parse --id <share-id|url> [--password <password>]");
    Console::println("  file download-url --id <docid> [--is-dir] [--name <name>] [--token <share-token>]");
    Console::println("  file batch-download-url --input <id[:file|dir],...> [--name <zip-name>] [--token <share-token>]");
    Console::println("  file upload --parent-id <docid> --path <path> [--name <name>] [--token <share-token>] [-y|--confirm|--yes]");
    Console::println("                                           真实云盘上传；支持秒传、小文件 PUT 和 20MiB 分片上传");
    Console::println("  app version                              显示应用版本信息");
    Console::println("  app announcement                         显示公告");
    Console::println("  grade list [--term <term-code>] [--all]  显示指定学期成绩");
    Console::println("  grade all                                显示全部成绩");

    Console::println("\n作业与课程签到");
    Console::println("  spoc week                                显示 SPOC 当前教学周和当前学期");
    Console::println("  spoc schedule --start-date <yyyy-MM-dd> --end-date <yyyy-MM-dd>");
    Console::println("                                           显示 SPOC 周课表；日期通常来自 spoc week 输出");
    Console::println("  spoc courses --term <term-code>          显示 SPOC 学期课程；term 通常来自 spoc week");
    Console::println("  spoc assignments [--pending-only] [--include-expired]");
    Console::println("                                           显示 SPOC 作业；详情 ID 在输出记录的 id 字段");
    Console::println("  spoc assignment show --id <assignment-id>");
    Console::println("                                           显示 SPOC 作业详情");
    Console::println("                                           来源: <assignment-id> 来自 spoc assignments 输出记录的 id 字段。");
    Console::println("  spoc homework submit --id <assignment-id> --course-id <course-id> --file-id <file-id> --name <file-name> [-y|--confirm|--yes]");
    Console::println("                                           提交已上传文件到 SPOC 作业；真实写操作必须确认");
    Console::println("  judge assignments [--course-id <course-id>] [--include-expired] [--include-history]");
    Console::println("                                           显示希冀作业；assignment-id 在输出记录的 id 字段");
    Console::println("  judge assignment show --assignment-id <assignment-id>");
    Console::println("                                           显示希冀作业概要；来源: judge assignments 输出记录的 id 字段");
    Console::println("  judge assignment details --assignment-id <assignment-id>");
    Console::println("                                           显示希冀作业详情；来源: judge assignments 输出记录的 id 字段");
    Console::println("  judge assignment details-batch --input <json|@file|ids>");
    Console::println("                                           批量显示希冀作业详情；ids 来自 judge assignments 输出记录的 id 字段");
    Console::println("  signin today                             显示今日课程签到；signin-id 在输出记录的 id 字段");
    Console::println("  signin schedule --date <yyyy-MM-dd>      显示指定日期课程签到");
    Console::println("  signin courses --term <term-code>        显示指定学期签到课程；course-id 在输出记录的 id 字段");
    Console::println("  signin course schedule --course-id <course-id>");
    Console::println("                                           显示单门课程签到明细；signin-id 在输出记录的 id 字段");
    Console::println("  signin do [--id <signin-id>|--course-id <course-id>] [-y|--confirm|--yes]");
    Console::println("                                           执行课程签到；来源: signin today 输出记录的 id 字段");
    Console::println("  wifi login [--username <account>] [--password <password>] [-y|--confirm|--yes]");
    Console::println("  wifi logout [--username <account>] [-y|--confirm|--yes]");
    Console::println("                                           BUAA-WiFi/BUAA-Mobile 网关写操作；不会自动 smoke");
    Console::println("  srs config                               显示选课系统配置和批次");
    Console::println("  srs batch                                解析当前预选批次 ID");
    Console::println("  srs course query [--scope <scope>] [--page <n>] [--size <n>] [--campus <1|2>] [--keyword <text>]");
    Console::println("                                           查询选课课程；clazzId 在 id 字段，secretVal 在 fields.secretVal");
    Console::println("  srs preselected                          查询预选结果");
    Console::println("  srs selected                             查询已选课程");
    Console::println("  srs course preselect --id <clazzId> --scope <scope> --token <secretVal> --batch-id <batchId> --index <n> [-y|--confirm|--yes]");
    Console::println("  srs course select --id <clazzId> --scope <scope> --token <secretVal> [-y|--confirm|--yes]");
    Console::println("  srs course drop --id <clazzId> --scope <scope> --token <secretVal> [-y|--confirm|--yes]");
    Console::println("                                           真实选课/退选写操作不会自动 smoke，需要用户显式确认");

    Console::println("\n博雅课程 (bykc)");
    Console::println("  bykc profile                             显示博雅课程档案");
    Console::println("  bykc courses [--page <n>] [--size <n>] [--keyword <text>]");
    Console::println("                                           显示可选博雅课程；course-id 在输出记录的 id 字段");
    Console::println("  bykc chosen                              显示已选博雅课程；course-id 在输出记录的 id 或 fields.courseId 字段");
    Console::println("  bykc stats                               显示博雅统计");
    Console::println("  bykc course show --course-id <course-id>");
    Console::println("                                           显示博雅课程详情；来源: bykc courses 或 bykc chosen 输出记录的 id 字段");
    Console::println("  bykc select --course-id <course-id> [-y|--confirm|--yes]");
    Console::println("                                           选择博雅课程；来源: bykc courses 输出记录的 id 字段");
    Console::println("  bykc unselect --course-id <course-id> [-y|--confirm|--yes]");
    Console::println("                                           退选博雅课程；来源: bykc chosen 输出记录的 id 字段");
    Console::println("  bykc sign --course-id <course-id> --sign-type <1|2> --lat <lat> --lng <lng> [-y|--confirm|--yes]");
    Console::println("                                           博雅签到/签退；必须显式提供真实坐标，不会默认伪造位置");

    Console::println("\n场馆预约 (cgyy)");
    Console::println("  cgyy sites                               显示场馆列表；site-id 在输出记录的 id 字段");
    Console::println("  cgyy purpose-types                       显示预约用途类型；purpose-type-id 在输出记录的 id 字段");
    Console::println("  cgyy day-info --site-id <site-id> --date <yyyy-MM-dd>");
    Console::println("                                           显示场馆日期可预约信息；来源: cgyy sites 输出记录的 id 字段");
    Console::println("  cgyy orders [--page <n>] [--size <n>]    显示场馆预约订单；order-id 在输出记录的 id 字段");
    Console::println("  cgyy order show --order-id <order-id>    显示场馆预约订单详情；来源: cgyy orders 输出记录的 id 字段");
    Console::println("  cgyy order lock-code [--order-id <order-id>]");
    Console::println("                                           显示场馆预约锁码；真实模式通常读取当前锁码");
    Console::println("  cgyy reserve --site-id <site-id> --space-id <space-id> --id <time-id> --date <yyyy-MM-dd> --purpose-type <purpose-type-id> --theme <theme> --phone <phone> --joiners <joiners> --captcha <captcha> --token <token> [-y|--confirm|--yes]");
    Console::println("                                           预约场馆；space-id 来自 cgyy day-info 的 id，time-id 来自 fields.timeId，token 来自 fields.token");
    Console::println("  cgyy order cancel --order-id <order-id> [-y|--confirm|--yes]");
    Console::println("                                           取消场馆预约；来源: cgyy orders 输出记录的 id 字段");

    Console::println("\n图书馆预约 (libbook)");
    Console::println("  libbook libraries                        显示图书馆列表；library-id 在输出记录的 id 字段");
    Console::println("  libbook areas --library-id <library-id> [--date <yyyy-MM-dd>] [--storey-id <storey-id>]");
    Console::println("                                           显示图书馆区域列表；来源: libbook libraries 输出记录的 id 字段");
    Console::println("  libbook seats --area-id <area-id> [--date <yyyy-MM-dd>] [--start-time <HH:mm>] [--end-time <HH:mm>]");
    Console::println("                                           显示座位列表；area-id 来自 libbook areas 输出记录的 id 字段");
    Console::println("  libbook reservations [--page <n>] [--size <n>]");
    Console::println("                                           显示图书馆座位预约记录；booking-id 在输出记录的 id 字段");
    Console::println("  libbook area show --area-id <area-id>");
    Console::println("                                           显示图书馆区域详情；来源: libbook areas 输出记录的 id 字段");
    Console::println("  libbook book --seat-id <seat-id> --date <yyyy-MM-dd> [--segment <segment>|--start-time <HH:mm> --end-time <HH:mm>] [-y|--confirm|--yes]");
    Console::println("                                           预约图书馆座位；来源: libbook seats 输出记录的 id 字段");
    Console::println("  libbook cancel --booking-id <booking-id> [-y|--confirm|--yes]");
    Console::println("                                           取消图书馆座位预约；来源: libbook reservations 输出记录的 id 字段");

    Console::println("\n阳光打卡、评教和待办");
    Console::println("  ygdk overview                            显示阳光打卡概览；item-id 在 status=item 的输出记录 id 字段");
    Console::println("  ygdk records [--page <n>] [--size <n>]   显示阳光打卡记录");
    Console::println("  ygdk submit [--item-id <item-id>] --start-time <time> --end-time <time> --place <place> --photo <path> [-y|--confirm|--yes]");
    Console::println("                                           提交阳光打卡；item-id 来自 ygdk overview 输出中 status=item 的记录 id 字段");
    Console::println("  evaluation list                          显示评教任务；evaluation-id 在输出记录的 id 字段");
    Console::println("  evaluation form --id <evaluation-id>");
    Console::println("                                           显示评教表单摘要；来源: evaluation list 输出记录的 id 字段");
    Console::println("  evaluation submit [--id <evaluation-id>] [-y|--confirm|--yes]");
    Console::println("                                           批量或指定提交评教；来源: evaluation list 输出记录的 id 字段");
    Console::println("  evaluation form submit --id <evaluation-id> [--reason <text>] [-y|--confirm|--yes]");
    Console::println("                                           提交指定评教表单；真实提交不会自动 smoke");
    Console::println("  todo list [--pending-only|--all]         显示待办聚合");

    Console::println("  td init [-y|--confirm|--yes]             初始化 TD 本地目录和默认配置");
    Console::println("  td image add <path> [--name <name>] [--overwrite] [-y|--confirm|--yes]");
    Console::println("                                           添加本地 TD 图片；不会上传到 TD 服务器");
    Console::println("  td image list                            列出本地 TD 图片");
    Console::println("  td user add <student-id> --quick <沙河|学院路> [-y|--confirm|--yes]");
    Console::println("                                           快速添加 TD 用户；需要先用 td image add 准备图片");
    Console::println("  td user add <student-id> --entrance <id> --exit <id> --entrance-image <name> --exit-image <name> [-y|--confirm|--yes]");
    Console::println("                                           显式添加 TD 用户；图片名来自 td image list");
    Console::println("  td user list                             列出本地 TD 用户");
    Console::println("  td user show <student-id>                显示本地 TD 用户详情");
    Console::println("  td user delete <student-id> [-y|--confirm|--yes]");
    Console::println("                                           删除本地 TD 用户");
    Console::println("  td status                                显示本地 TD 状态缓存");
    Console::println("  td count [student-id]                    读取本地缓存的 TD 锻炼次数，不访问服务器");
    Console::println("  td count [student-id] --refresh [-y|--confirm|--yes]");
    Console::println("                                           确认后通过 TD check 协议刷新服务器次数，具有写语义");
    Console::println("  td run --once [-y|--confirm|--yes]       确认后执行一次 TD 入口/出口编排并上传图片");
    Console::println("  td scheduler once [-y|--confirm|--yes]  执行一次 TD 时间窗口调度并写入日志");
    Console::println("  td scheduler clear-errors [--date <yyyy-MM-dd>] [-y|--confirm|--yes]");
    Console::println("                                           清理今日或指定日期的 TD error 状态");
    Console::println("  td scheduler watch [--poll-seconds <n>] [-y|--confirm|--yes]");
    Console::println("                                           持续后台轮询 TD 调度；人类可读模式按 Ctrl-C 停止");

    Console::println("\n配置、缓存和占位接口");
    Console::println("  capability show                          显示当前平台能力声明");
    Console::println("  config show                              显示当前配置");
    Console::println("  config set --key <key> --value <value> [-y|--confirm|--yes]");
    Console::println("                                           设置配置项；key 支持 mode、proxy、cache");
    Console::println("  cache clear [-y|--confirm|--yes]                    清除缓存");
    Console::println("  file upload --parent-id <docid> --path <path> [-y|--confirm|--yes]");
    Console::println("                                           上传本地文件到北航云盘，写操作必须确认");

    Console::println("\n常用选项");
    Console::println("  --json                                   JSON 格式输出");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  --mock                                   使用模拟数据");
#endif
    Console::println("  --mode vpn|direct                        临时覆盖连接模式（未指定时使用配置，默认 vpn）");

    Console::println("\n配置键");
    Console::println("  mode      连接模式 (vpn|direct)");
    Console::println("  proxy     代理地址 (url 或空)");
    Console::println("  cache     缓存开关 (true|false)");
}

void print_help(OutputFormatter &out) {
    if (out.is_json()) {
        Console::println("{}", get_help_json().dump(2));
    } else {
        print_usage();
    }
}

bool is_cli_command(const std::string &command) {
    static constexpr std::array<std::string_view, 26> commands = {
        "course", "exam", "classroom", "term", "week", "live", "config", "mode", "cache",
        "user", "app", "grade", "spoc", "judge", "signin", "ygdk", "evaluation",
        "bykc", "cgyy", "libbook", "todo", "file", "wifi", "srs", "td", "capability",
    };
    return std::find(commands.begin(), commands.end(), std::string_view(command)) != commands.end();
}

bool is_command_with_action(const std::string &command) {
    static constexpr std::array<std::string_view, 9> commands = {"spoc", "judge", "signin", "bykc", "cgyy", "libbook", "srs", "td", "evaluation"};
    return std::find(commands.begin(), commands.end(), std::string_view(command)) != commands.end();
}

} // namespace UBAANextCli
