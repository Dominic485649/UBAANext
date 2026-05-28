#include "CommandHandlers.hpp"

#include "Console.hpp"
#include "OutputFormatter.hpp"

#include <UBAANext/Version.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <algorithm>
#include <string>
#include <string_view>

namespace UBAANextCli {
namespace {

using json = nlohmann::json;

void add_command(json &commands, const std::string &name, const std::string &description, const json &options = {}) {
    json command = {{"name", name}, {"description", description}};
    if (!options.empty()) {
        command["options"] = options;
    }
    commands.push_back(command);
}

} // namespace

nlohmann::json get_help_json() {
    json commands = json::array();

    add_command(commands, "version", "显示版本信息");
    add_command(commands, "help", "显示帮助信息");

    json login_options = {
        {{"name", "account"}, {"description", "账号/学号位置参数"}, {"required", true}},
        {{"name", "password"}, {"description", "密码位置参数"}, {"required", true}},
        {{"name", "--username"}, {"description", "兼容旧写法：学号"}, {"required", false}},
        {{"name", "--password"}, {"description", "兼容旧写法：密码"}, {"required", false}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "login", "登录", login_options);

    add_command(commands, "mode", "显示当前连接模式");
    add_command(commands, "mode vpn", "切换为 VPN 模式");
    add_command(commands, "mode direct", "切换为直连模式");

    add_command(commands, "whoami", "显示当前用户信息");

    json course_options = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "course today", "显示今天的课程", course_options);

    json course_date_options = {
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "course date", "显示指定日期的课程", course_date_options);

    json course_week_options = {
        {{"name", "--week"}, {"description", "周次 (1-30)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "course week", "显示指定周次的课程", course_week_options);

    json exam_options = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "exam list", "显示考试列表", exam_options);

    json classroom_options = {
        {{"name", "--campus"}, {"description", "校区 ID (1-10)"}, {"required", true}},
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "classroom query", "查询空闲教室", classroom_options);

    json term_options = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_command(commands, "term list", "显示学期列表", term_options);
    add_command(commands, "week list", "显示教学周列表", term_options);

    add_command(commands, "config show", "显示当前配置");

    json config_set_options = {
        {{"name", "--key"}, {"description", "配置键"}, {"required", true}},
        {{"name", "--value"}, {"description", "配置值"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认修改本地配置"}, {"required", true}},
    };
    add_command(commands, "config set", "设置配置项", config_set_options);

    json confirm_options = {
        {{"name", "--confirm"}, {"description", "确认执行有副作用操作"}, {"required", true}},
    };
    add_command(commands, "cache clear", "清除缓存", confirm_options);
    add_command(commands, "logout", "登出并清除本地会话", confirm_options);
    add_command(commands, "user info", "显示用户信息");
    add_command(commands, "app version", "显示应用版本信息");
    add_command(commands, "app announcement", "显示公告");
    json grade_list_options = {
        {{"name", "--term"}, {"description", "学期代码"}, {"required", false}},
        {{"name", "--all"}, {"description", "显示全部成绩"}, {"required", false}},
    };
    add_command(commands, "grade list", "显示指定学期成绩", grade_list_options);
    add_command(commands, "grade all", "显示全部成绩");
    add_command(commands, "spoc assignments", "显示 SPOC 作业", json{
        {{"name", "--pending-only"}, {"description", "只显示待处理作业"}, {"required", false}},
        {{"name", "--include-expired"}, {"description", "包含已过期作业"}, {"required", false}},
    });
    add_command(commands, "spoc assignment show", "显示 SPOC 作业详情", json{
        {{"name", "--id"}, {"description", "SPOC 作业 ID"}, {"required", true}},
    });
    add_command(commands, "judge assignments", "显示希冀作业", json{
        {{"name", "--course-id"}, {"description", "课程 ID"}, {"required", false}},
        {{"name", "--include-expired"}, {"description", "包含已过期作业"}, {"required", false}},
        {{"name", "--include-history"}, {"description", "包含历史作业"}, {"required", false}},
    });
    add_command(commands, "judge assignment show", "显示希冀作业概要", json{
        {{"name", "--assignment-id"}, {"description", "希冀作业 ID"}, {"required", true}},
    });
    add_command(commands, "judge assignment details", "显示希冀作业详情", json{
        {{"name", "--assignment-id"}, {"description", "希冀作业 ID"}, {"required", true}},
    });
    add_command(commands, "judge assignment details-batch", "批量显示希冀作业详情", json{
        {{"name", "--input"}, {"description", "JSON、@file 或逗号分隔作业 ID"}, {"required", true}},
    });
    add_command(commands, "signin today", "显示今日课程签到");
    add_command(commands, "signin do", "执行课程签到", json{
        {{"name", "--id"}, {"description", "签到任务 ID"}, {"required", false}},
        {{"name", "--course-id"}, {"description", "课程 ID"}, {"required", false}},
        {{"name", "--confirm"}, {"description", "确认执行签到"}, {"required", true}},
    });
    add_command(commands, "bykc profile", "显示博雅资料");
    add_command(commands, "bykc courses", "显示博雅课程");
    add_command(commands, "bykc chosen", "显示已选博雅课程");
    add_command(commands, "bykc stats", "显示博雅统计");
    add_command(commands, "bykc course show", "显示博雅课程详情", json{
        {{"name", "--course-id"}, {"description", "博雅课程 ID"}, {"required", true}},
    });
    add_command(commands, "bykc select", "选择博雅课程", json{
        {{"name", "--course-id"}, {"description", "博雅课程 ID"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认选择"}, {"required", true}},
    });
    add_command(commands, "bykc unselect", "退选博雅课程", json{
        {{"name", "--course-id"}, {"description", "博雅课程 ID"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认退选"}, {"required", true}},
    });
    add_command(commands, "bykc sign", "博雅签到/签退", json{
        {{"name", "--course-id"}, {"description", "博雅课程 ID"}, {"required", true}},
        {{"name", "--sign-type"}, {"description", "签到类型"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认执行签到/签退"}, {"required", true}},
    });
    add_command(commands, "cgyy sites", "显示场馆列表");
    add_command(commands, "cgyy purpose-types", "显示场馆预约用途类型");
    add_command(commands, "cgyy day-info", "显示场馆日期可预约信息", json{
        {{"name", "--site-id"}, {"description", "场馆 ID"}, {"required", true}},
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
    });
    add_command(commands, "cgyy orders", "显示场馆预约订单");
    add_command(commands, "cgyy order show", "显示场馆预约订单详情", json{
        {{"name", "--order-id"}, {"description", "订单 ID"}, {"required", true}},
    });
    add_command(commands, "cgyy order lock-code", "显示场馆预约锁码", json{
        {{"name", "--order-id"}, {"description", "订单 ID"}, {"required", false}},
    });
    add_command(commands, "cgyy reserve", "预约场馆", json{
        {{"name", "--site-id"}, {"description", "场馆 ID"}, {"required", true}},
        {{"name", "--space-id"}, {"description", "场地 ID"}, {"required", true}},
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
        {{"name", "--id"}, {"description", "预约时段 ID"}, {"required", true}},
        {{"name", "--purpose-type"}, {"description", "用途类型 ID"}, {"required", true}},
        {{"name", "--theme"}, {"description", "预约主题"}, {"required", true}},
        {{"name", "--phone"}, {"description", "联系电话"}, {"required", true}},
        {{"name", "--joiners"}, {"description", "参与人"}, {"required", true}},
        {{"name", "--captcha"}, {"description", "验证码"}, {"required", true}},
        {{"name", "--token"}, {"description", "预约上下文 token"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认预约"}, {"required", true}},
    });
    add_command(commands, "cgyy order cancel", "取消场馆预约", json{
        {{"name", "--order-id"}, {"description", "订单 ID"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认取消"}, {"required", true}},
    });
    add_command(commands, "libbook libraries", "显示图书馆列表");
    add_command(commands, "libbook areas", "显示图书馆区域列表", json{
        {{"name", "--library-id"}, {"description", "图书馆 ID"}, {"required", true}},
        {{"name", "--day"}, {"description", "日期"}, {"required", false}},
        {{"name", "--storey-id"}, {"description", "楼层 ID"}, {"required", false}},
    });
    add_command(commands, "libbook seats", "显示座位列表", json{
        {{"name", "--area-id"}, {"description", "区域 ID"}, {"required", true}},
        {{"name", "--day"}, {"description", "日期"}, {"required", false}},
        {{"name", "--start-time"}, {"description", "开始时间"}, {"required", false}},
        {{"name", "--end-time"}, {"description", "结束时间"}, {"required", false}},
    });
    add_command(commands, "libbook reservations", "显示图书馆座位预约记录", json{
        {{"name", "--page"}, {"description", "页码"}, {"required", false}},
        {{"name", "--size"}, {"description", "每页数量"}, {"required", false}},
    });
    add_command(commands, "libbook area show", "显示图书馆区域详情", json{
        {{"name", "--area-id"}, {"description", "区域 ID"}, {"required", true}},
    });
    add_command(commands, "libbook book", "预约图书馆座位", json{
        {{"name", "--seat-id"}, {"description", "座位 ID"}, {"required", true}},
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
        {{"name", "--segment"}, {"description", "预约时段"}, {"required", false}},
        {{"name", "--start-time"}, {"description", "开始时间"}, {"required", false}},
        {{"name", "--end-time"}, {"description", "结束时间"}, {"required", false}},
        {{"name", "--confirm"}, {"description", "确认预约"}, {"required", true}},
    });
    add_command(commands, "libbook cancel", "取消图书馆座位预约", json{
        {{"name", "--booking-id"}, {"description", "预约 ID"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认取消"}, {"required", true}},
    });
    add_command(commands, "ygdk overview", "显示阳光打卡概览");
    add_command(commands, "ygdk records", "显示阳光打卡记录", json{
        {{"name", "--page"}, {"description", "页码"}, {"required", false}},
        {{"name", "--size"}, {"description", "每页数量"}, {"required", false}},
    });
    add_command(commands, "ygdk submit", "提交阳光打卡", json{
        {{"name", "--id"}, {"description", "打卡项目 ID"}, {"required", false}},
        {{"name", "--item-id"}, {"description", "打卡项目 ID"}, {"required", false}},
        {{"name", "--start-time"}, {"description", "开始时间"}, {"required", true}},
        {{"name", "--end-time"}, {"description", "结束时间"}, {"required", true}},
        {{"name", "--place"}, {"description", "地点"}, {"required", true}},
        {{"name", "--photo"}, {"description", "照片路径"}, {"required", true}},
        {{"name", "--share"}, {"description", "同步共享"}, {"required", false}},
        {{"name", "--confirm"}, {"description", "确认提交"}, {"required", true}},
    });
    add_command(commands, "evaluation list", "显示评教任务");
    add_command(commands, "evaluation submit", "提交评教", json{
        {{"name", "--id"}, {"description", "评教任务 ID"}, {"required", false}},
        {{"name", "--confirm"}, {"description", "确认提交评教"}, {"required", true}},
    });
    add_command(commands, "todo list", "显示待办聚合", json{
        {{"name", "--pending-only"}, {"description", "只显示待处理项目"}, {"required", false}},
        {{"name", "--all"}, {"description", "包含非待处理项目"}, {"required", false}},
    });
    add_command(commands, "file upload", "保留的文件/附件上传接口，当前稳定返回 NotImplemented", json{
        {{"name", "--path"}, {"description", "待上传文件路径"}, {"required", true}},
        {{"name", "--confirm"}, {"description", "确认执行上传类有副作用操作"}, {"required", true}},
    });

    return {{"ok", true}, {"data", {{"commands", commands}, {"version", UBAANEXT_VERSION_STRING}}}, {"error", nullptr}};
}

void print_usage() {
    Console::println("用法: ubaa <command> [options]\n");
    Console::println("命令:");
    Console::println("  version                          显示版本");
    Console::println("  help                             显示帮助");
    Console::println("  login <id> <pw>");
    Console::println("                                   登录（默认 VPN 模式，兼容 --username/--password）");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  login --mock <id> <pw>");
    Console::println("                                   模拟登录");
#endif
    Console::println("  mode                             显示当前连接模式");
    Console::println("  mode direct                      切换为直连模式");
    Console::println("  mode vpn                         切换为 VPN 模式");
    Console::println("  whoami                           显示当前用户");
    Console::println("  logout --confirm                 登出并清除本地会话");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  course today [--mock]            显示今天的课程");
#else
    Console::println("  course today                     显示今天的课程");
#endif
    Console::println("  course date --date <yyyy-MM-dd>  显示指定日期课程");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  course week [--mock] --week <n>  显示指定周次课程");
#else
    Console::println("  course week --week <n>           显示指定周次课程");
#endif
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  exam list [--mock]               显示考试");
#else
    Console::println("  exam list                        显示考试");
#endif
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  classroom query [--mock] --campus <id> --date <yyyy-MM-dd>");
#else
    Console::println("  classroom query --campus <id> --date <yyyy-MM-dd>");
#endif
    Console::println("                                   查询空闲教室");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  term list [--mock]               显示学期列表");
    Console::println("  week list [--mock]               显示教学周列表");
#else
    Console::println("  term list                        显示学期列表");
    Console::println("  week list                        显示教学周列表");
#endif
    Console::println("\n用户、应用与教学基础:");
    Console::println("  user info                        显示用户信息");
    Console::println("  app version                      显示应用版本信息");
    Console::println("  app announcement                 显示公告");
    Console::println("  grade list [--term <term>] [--all]");
    Console::println("                                   显示指定学期成绩");
    Console::println("  grade all                        显示全部成绩");
    Console::println("\n作业与课程签到:");
    Console::println("  spoc assignments [--pending-only] [--include-expired]");
    Console::println("                                   显示 SPOC 作业");
    Console::println("  spoc assignment show --id <id>   显示 SPOC 作业详情");
    Console::println("  judge assignments [--course-id <id>] [--include-expired] [--include-history]");
    Console::println("                                   显示希冀作业");
    Console::println("  judge assignment show --assignment-id <id>");
    Console::println("                                   显示希冀作业概要");
    Console::println("  judge assignment details --assignment-id <id>");
    Console::println("                                   显示希冀作业详情");
    Console::println("  judge assignment details-batch --input <json|@file|ids>");
    Console::println("                                   批量显示希冀作业详情");
    Console::println("  signin today                     显示今日课程签到");
    Console::println("  signin do [--id <id>|--course-id <id>] --confirm");
    Console::println("                                   执行课程签到");
    Console::println("\n阳光打卡与评教:");
    Console::println("  ygdk overview                    显示阳光打卡概览");
    Console::println("  ygdk records [--page <n>] [--size <n>]");
    Console::println("                                   显示阳光打卡记录");
    Console::println("  ygdk submit --start-time <time> --end-time <time> --place <place> --photo <path> --confirm");
    Console::println("                                   提交阳光打卡");
    Console::println("  evaluation list                  显示评教任务");
    Console::println("  evaluation submit [--id <id>] --confirm");
    Console::println("                                   提交评教");
    Console::println("\n博雅课程 (bykc):");
    Console::println("  bykc profile                     显示博雅课程档案");
    Console::println("  bykc courses [--page <n>] [--size <n>] [--keyword <kw>]");
    Console::println("                                   显示可选博雅课程");
    Console::println("  bykc chosen                      显示已选博雅课程");
    Console::println("  bykc stats                       显示博雅统计");
    Console::println("  bykc course show --course-id <id>");
    Console::println("                                   显示博雅课程详情");
    Console::println("  bykc select --course-id <id> --confirm");
    Console::println("                                   选择博雅课程");
    Console::println("  bykc unselect --course-id <id> --confirm");
    Console::println("                                   退选博雅课程");
    Console::println("  bykc sign --course-id <id> --sign-type <type> --confirm");
    Console::println("                                   博雅签到/签退");
    Console::println("\n场馆预约 (cgyy):");
    Console::println("  cgyy sites                       显示场馆列表");
    Console::println("  cgyy purpose-types               显示场馆预约用途类型");
    Console::println("  cgyy day-info --site-id <id> --date <yyyy-MM-dd>");
    Console::println("                                   显示场馆日期可预约信息");
    Console::println("  cgyy orders                      显示场馆预约订单");
    Console::println("  cgyy order show --order-id <id>  显示场馆预约订单详情");
    Console::println("  cgyy order lock-code             显示场馆预约锁码");
    Console::println("  cgyy reserve --site-id <id> --space-id <id> --date <yyyy-MM-dd> --confirm");
    Console::println("                                   预约场馆");
    Console::println("  cgyy order cancel --order-id <id> --confirm");
    Console::println("                                   取消场馆预约");
    Console::println("\n图书馆预约 (libbook):");
    Console::println("  libbook libraries                显示图书馆列表");
    Console::println("  libbook areas --library-id <id>  显示图书馆区域列表");
    Console::println("  libbook seats --area-id <id>     显示图书馆座位列表");
    Console::println("  libbook reservations             显示图书馆座位预约记录");
    Console::println("  libbook area show --area-id <id>");
    Console::println("                                   显示图书馆区域详情");
    Console::println("  libbook book --seat-id <id> --date <yyyy-MM-dd> --confirm");
    Console::println("                                   预约图书馆座位");
    Console::println("  libbook cancel --booking-id <id> --confirm");
    Console::println("                                   取消图书馆座位预约");
    Console::println("\n待办和占位接口:");
    Console::println("  todo list [--pending-only|--all] 显示待办聚合");
    Console::println("  file upload --path <path> --confirm");
    Console::println("                                   保留接口，当前返回 NotImplemented");
    Console::println("\n其他:");
    Console::println("  config show                      显示当前配置");
    Console::println("  config set --key <key> --value <value> --confirm");
    Console::println("                                   设置配置项");
    Console::println("  cache clear --confirm            清除缓存");
    Console::println("\n选项:");
    Console::println("  --json                           JSON 格式输出");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  --mock                           使用模拟数据");
#endif
    Console::println("  --mode vpn|direct                临时覆盖连接模式（未指定时使用配置，默认 vpn）");
    Console::println("\n配置键:");
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
    static constexpr std::array<std::string_view, 21> commands = {
        "course", "exam", "classroom", "term", "week", "config", "mode", "cache",
        "user", "app", "grade", "spoc", "judge", "signin", "ygdk", "evaluation",
        "bykc", "cgyy", "libbook", "todo", "file",
    };
    return std::find(commands.begin(), commands.end(), std::string_view(command)) != commands.end();
}

bool is_command_with_action(const std::string &command) {
    static constexpr std::array<std::string_view, 5> commands = {"spoc", "judge", "bykc", "cgyy", "libbook"};
    return std::find(commands.begin(), commands.end(), std::string_view(command)) != commands.end();
}

} // namespace UBAANextCli
