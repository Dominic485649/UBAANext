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
        {{"name", "--username"}, {"description", "学号"}, {"required", true}},
        {{"name", "--password"}, {"description", "密码"}, {"required", true}},
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
    add_command(commands, "grade list", "显示指定学期成绩");
    add_command(commands, "grade all", "显示全部成绩");
    add_command(commands, "spoc assignments", "显示 SPOC 作业");
    add_command(commands, "spoc assignment show", "显示 SPOC 作业详情");
    add_command(commands, "judge assignments", "显示希冀作业");
    add_command(commands, "judge assignment show", "显示希冀作业概要");
    add_command(commands, "judge assignment details", "显示希冀作业详情");
    add_command(commands, "judge assignment details-batch", "批量显示希冀作业详情");
    add_command(commands, "signin today", "显示今日签到");
    add_command(commands, "signin do", "执行签到", confirm_options);
    add_command(commands, "bykc profile", "显示博雅资料");
    add_command(commands, "bykc courses", "显示博雅课程");
    add_command(commands, "bykc chosen", "显示已选博雅课程");
    add_command(commands, "bykc stats", "显示博雅统计");
    add_command(commands, "bykc select", "选择博雅课程", confirm_options);
    add_command(commands, "bykc unselect", "退选博雅课程", confirm_options);
    add_command(commands, "bykc sign", "博雅签到/签退", confirm_options);
    add_command(commands, "cgyy sites", "显示场馆列表");
    add_command(commands, "cgyy day-info", "显示场馆日期可预约信息");
    add_command(commands, "cgyy reserve", "预约场馆", confirm_options);
    add_command(commands, "cgyy order cancel", "取消场馆预约", confirm_options);
    add_command(commands, "libbook libraries", "显示图书馆列表");
    add_command(commands, "libbook seats", "显示座位列表");
    add_command(commands, "libbook book", "预约图书馆座位", confirm_options);
    add_command(commands, "libbook cancel", "取消图书馆座位预约", confirm_options);
    add_command(commands, "ygdk overview", "显示阳光打卡概览");
    add_command(commands, "ygdk records", "显示阳光打卡记录");
    add_command(commands, "ygdk submit", "提交阳光打卡", confirm_options);
    add_command(commands, "evaluation list", "显示评教任务");
    add_command(commands, "evaluation submit", "提交评教", confirm_options);
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
    Console::println("  login --username <id> --password <pw>");
    Console::println("                                   登录（默认 VPN 模式）");
#if UBAANEXT_ENABLE_MOCKS
    Console::println("  login --mock --username <id> --password <pw>");
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
    Console::println("\n作业、待办和占位接口:");
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
