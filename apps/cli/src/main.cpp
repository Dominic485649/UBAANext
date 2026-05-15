/**
 * @file main.cpp
 * @brief UBAA Next 命令行界面入口
 *
 * 命令树：
 *   version [--json]
 *   help [--json]
 *   login [--mock] --username <id> --password <pw> [--mode vpn|direct]
 *   whoami [--json]
 *   logout [--json]
 *   course today [--mock] [--mode vpn|direct] [--json]
 *   course date --date <yyyy-MM-dd> [--mock] [--mode vpn|direct] [--json]
 *   course week --week <n> [--mock] [--mode vpn|direct] [--json]
 *   exam list [--mock] [--mode vpn|direct] [--json]
 *   classroom query --campus <id> --date <yyyy-MM-dd> [--mock] [--mode vpn|direct] [--json]
 *   term list [--mock] [--mode vpn|direct] [--json]
 *   week list [--mock] [--mode vpn|direct] [--json]
 *   config show [--json]
 *   config set --key <key> --value <value> [--json]
 *   cache clear [--json]
 *
 * 所有命令支持 --json 输出。
 */

#include "AppContext.hpp"
#include "CliConfig.hpp"
#include "Console.hpp"
#include "ExitCodes.hpp"
#include "OutputFormatter.hpp"
#include "PlainFileStore.hpp"
#include "ServiceFactory.hpp"

#include <UBAANext/Version.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#if defined(_WIN32)
#include <UBAANext/Net/WinHttpClient.hpp>
#endif

#if UBAANEXT_ENABLE_MOCKS
#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>
#endif

#include <nlohmann/json.hpp>

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace um = UBAANext;
using UBAANextCli::AppContext;
using UBAANextCli::CliConfig;
using UBAANextCli::ExitCode;
using UBAANextCli::OutputFormatter;
using UBAANextCli::ServiceFactory;

// ── CLI 参数结构 ──────────────────────────────────────────────

struct CliArgs {
    std::string command;
    std::string subcommand;
    std::string action;
    bool mock = false;
    bool json_output = false;
    bool parse_error = false;
    std::string username;
    std::string password;
    int week = 0;
    int campus = 0;
    std::string date;
    std::string config_key;
    std::string config_value;
    std::string mode;  // "vpn" (default) or "direct"
    std::string term;
    std::string id;
    std::string course_id;
    std::string assignment_id;
    std::string area_id;
    std::string library_id;
    std::string booking_id;
    std::string order_id;
    std::string site_id;
    std::string space_id;
    std::string purpose_type;
    std::string theme;
    std::string phone;
    std::string joiners;
    std::string captcha;
    std::string token;
    std::string item_id;
    std::string place;
    std::string photo_path;
    std::string seat_id;
    std::string segment;
    std::string sections;
    std::string input;
    int page = 1;
    int size = 20;
    bool all = false;
    std::string status;
    std::string category;
    std::string sub_category;
    std::string keyword;
    bool include_expired = false;
    bool include_history = false;
    bool pending_only = false;
    bool share = false;
    bool has_lat = false;
    bool has_lng = false;
    double lat = 0.0;
    double lng = 0.0;
    int sign_type = 0;
    std::string start_time;
    std::string end_time;
    std::string storey_id;
    bool confirmed = false;
    std::string error_message;  // 详细的错误信息
};

// ── 参数解析 ──────────────────────────────────────────────────

[[nodiscard]] std::optional<int> parse_int(std::string_view sv) noexcept {
    int value = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec == std::errc{} && ptr == sv.data() + sv.size()) {
        return value;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<double> parse_double(std::string_view sv) {
    try {
        std::string text(sv);
        size_t pos = 0;
        double value = std::stod(text, &pos);
        if (pos == text.size()) {
            return value;
        }
    } catch (...) {
    }
    return std::nullopt;
}

[[nodiscard]] bool is_valid_date(const std::string &date) {
    if (date.size() != 10) return false;
    if (date[4] != '-' || date[7] != '-') return false;
    // 简单格式校验：yyyy-MM-dd
    for (int i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) continue;
        if (date[i] < '0' || date[i] > '9') return false;
    }
    return true;
}

[[nodiscard]] bool is_valid_mode(const std::string &mode) {
    return mode == "vpn" || mode == "direct";
}

[[nodiscard]] std::optional<std::vector<int>> parse_sections_arg(const std::string &text) {
    std::vector<int> sections;
    std::string current;
    for (char ch : text) {
        if (ch == ',') {
            if (current.empty()) return std::nullopt;
            auto parsed = parse_int(current);
            if (!parsed || *parsed < 1 || *parsed > 20) return std::nullopt;
            sections.push_back(*parsed);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        auto parsed = parse_int(current);
        if (!parsed || *parsed < 1 || *parsed > 20) return std::nullopt;
        sections.push_back(*parsed);
    }
    return sections;
}

CliArgs parse_args(int argc, char *argv[]) {
    CliArgs args;

    if (argc < 2) {
        return args;
    }

    args.command = argv[1];

    int i = 2;
    if ((args.command == "course" || args.command == "exam" ||
         args.command == "classroom" || args.command == "term" ||
         args.command == "week" || args.command == "config" ||
         args.command == "cache" || args.command == "user" ||
         args.command == "app" || args.command == "grade" ||
         args.command == "spoc" || args.command == "judge" ||
         args.command == "signin" || args.command == "ygdk" ||
         args.command == "evaluation" || args.command == "bykc" ||
         args.command == "cgyy" || args.command == "libbook" ||
         args.command == "todo") &&
        i < argc) {
        args.subcommand = argv[i];
        ++i;
        if ((args.command == "spoc" || args.command == "judge" || args.command == "bykc" ||
             args.command == "cgyy" || args.command == "libbook") &&
            i < argc && std::string_view(argv[i]).find("--") != 0) {
            args.action = argv[i];
            ++i;
        }
    }

    for (; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--mock") {
#if UBAANEXT_ENABLE_MOCKS
            args.mock = true;
#else
            args.error_message = "Release 构建不支持 --mock";
            args.parse_error = true;
#endif
        } else if (arg == "--json") {
            args.json_output = true;
        } else if (arg == "--username" && i + 1 < argc) {
            args.username = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            args.password = argv[++i];
        } else if (arg == "--week" && i + 1 < argc) {
            if (auto v = parse_int(argv[++i])) {
                if (*v < 1 || *v > 30) {
                    args.error_message = "--week 值必须在 1-30 之间";
                    args.parse_error = true;
                } else {
                    args.week = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--week 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--campus" && i + 1 < argc) {
            if (auto v = parse_int(argv[++i])) {
                if (*v < 1 || *v > 10) {
                    args.error_message = "--campus 值必须在 1-10 之间";
                    args.parse_error = true;
                } else {
                    args.campus = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--campus 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--date" && i + 1 < argc) {
            args.date = argv[++i];
            if (!is_valid_date(args.date)) {
                args.error_message = UBAANextCli::Console::format("--date 格式无效 '{}'，应为 yyyy-MM-dd", args.date);
                args.parse_error = true;
            }
        } else if (arg == "--key" && i + 1 < argc) {
            args.config_key = argv[++i];
        } else if (arg == "--value" && i + 1 < argc) {
            args.config_value = argv[++i];
        } else if (arg == "--mode" && i + 1 < argc) {
            args.mode = argv[++i];
            if (!is_valid_mode(args.mode)) {
                args.error_message = UBAANextCli::Console::format("--mode 值无效 '{}'，应为 vpn 或 direct", args.mode);
                args.parse_error = true;
            }
        } else if (arg == "--term" && i + 1 < argc) {
            args.term = argv[++i];
        } else if (arg == "--id" && i + 1 < argc) {
            args.id = argv[++i];
        } else if (arg == "--course-id" && i + 1 < argc) {
            args.course_id = argv[++i];
        } else if (arg == "--assignment-id" && i + 1 < argc) {
            args.assignment_id = argv[++i];
        } else if (arg == "--area-id" && i + 1 < argc) {
            args.area_id = argv[++i];
        } else if (arg == "--library-id" && i + 1 < argc) {
            args.library_id = argv[++i];
        } else if (arg == "--booking-id" && i + 1 < argc) {
            args.booking_id = argv[++i];
        } else if (arg == "--order-id" && i + 1 < argc) {
            args.order_id = argv[++i];
        } else if (arg == "--site-id" && i + 1 < argc) {
            args.site_id = argv[++i];
        } else if (arg == "--page" && i + 1 < argc) {
            if (auto v = parse_int(argv[++i])) {
                if (*v < 1) {
                    args.error_message = "--page 值必须大于等于 1";
                    args.parse_error = true;
                } else {
                    args.page = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--page 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if ((arg == "--size" || arg == "--limit") && i + 1 < argc) {
            if (auto v = parse_int(argv[++i])) {
                if (*v < 1 || *v > 200) {
                    args.error_message = "--size 值必须在 1-200 之间";
                    args.parse_error = true;
                } else {
                    args.size = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--size 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--all") {
            args.all = true;
        } else if (arg == "--status" && i + 1 < argc) {
            args.status = argv[++i];
        } else if (arg == "--category" && i + 1 < argc) {
            args.category = argv[++i];
        } else if ((arg == "--sub-category" || arg == "--subcategory") && i + 1 < argc) {
            args.sub_category = argv[++i];
        } else if (arg == "--keyword" && i + 1 < argc) {
            args.keyword = argv[++i];
        } else if (arg == "--start-time" && i + 1 < argc) {
            args.start_time = argv[++i];
        } else if (arg == "--end-time" && i + 1 < argc) {
            args.end_time = argv[++i];
        } else if (arg == "--storey-id" && i + 1 < argc) {
            args.storey_id = argv[++i];
        } else if (arg == "--space-id" && i + 1 < argc) {
            args.space_id = argv[++i];
        } else if (arg == "--purpose-type" && i + 1 < argc) {
            args.purpose_type = argv[++i];
        } else if (arg == "--theme" && i + 1 < argc) {
            args.theme = argv[++i];
        } else if (arg == "--phone" && i + 1 < argc) {
            args.phone = argv[++i];
        } else if (arg == "--joiners" && i + 1 < argc) {
            args.joiners = argv[++i];
        } else if (arg == "--captcha" && i + 1 < argc) {
            args.captcha = argv[++i];
        } else if (arg == "--token" && i + 1 < argc) {
            args.token = argv[++i];
        } else if (arg == "--item-id" && i + 1 < argc) {
            args.item_id = argv[++i];
        } else if (arg == "--place" && i + 1 < argc) {
            args.place = argv[++i];
        } else if (arg == "--photo" && i + 1 < argc) {
            args.photo_path = argv[++i];
        } else if (arg == "--seat-id" && i + 1 < argc) {
            args.seat_id = argv[++i];
        } else if (arg == "--segment" && i + 1 < argc) {
            args.segment = argv[++i];
        } else if (arg == "--sections" && i + 1 < argc) {
            args.sections = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            args.input = argv[++i];
        } else if (arg == "--include-expired") {
            args.include_expired = true;
        } else if (arg == "--include-history") {
            args.include_history = true;
        } else if (arg == "--pending-only") {
            args.pending_only = true;
        } else if (arg == "--share") {
            args.share = true;
        } else if (arg == "--lat" && i + 1 < argc) {
            if (auto v = parse_double(argv[++i])) {
                args.lat = *v;
                args.has_lat = true;
            } else {
                args.error_message = UBAANextCli::Console::format("--lat 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--lng" && i + 1 < argc) {
            if (auto v = parse_double(argv[++i])) {
                args.lng = *v;
                args.has_lng = true;
            } else {
                args.error_message = UBAANextCli::Console::format("--lng 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--sign-type" && i + 1 < argc) {
            if (auto v = parse_int(argv[++i])) {
                if (*v != 1 && *v != 2) {
                    args.error_message = "--sign-type 只能为 1(签到) 或 2(签退)";
                    args.parse_error = true;
                } else {
                    args.sign_type = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--sign-type 值无效 '{}'", argv[i]);
                args.parse_error = true;
            }
        } else if (arg == "--confirm" || arg == "--yes") {
            args.confirmed = true;
        } else if (arg == "--base-url" && i + 1 < argc) {
            // 兼容旧参数
            args.config_key = "base-url";
            args.config_value = argv[++i];
        } else if (arg == "--proxy" && i + 1 < argc) {
            // 兼容旧参数
            args.config_key = "proxy";
            args.config_value = argv[++i];
        } else if (arg.rfind("--", 0) == 0) {
            args.error_message = UBAANextCli::Console::format("未知选项: '{}'", arg);
            args.parse_error = true;
        } else {
            args.error_message = UBAANextCli::Console::format("未知参数: '{}'", arg);
            args.parse_error = true;
        }
    }

    return args;
}

// ── 路径工具 ──────────────────────────────────────────────────

[[nodiscard]] std::filesystem::path get_app_data_dir() {
#if defined(_WIN32)
    char *buf = nullptr;
    std::size_t len = 0;
    if (_dupenv_s(&buf, &len, "LOCALAPPDATA") == 0 && buf != nullptr) {
        std::filesystem::path path = std::filesystem::path(buf) / "UBAANext";
        free(buf);
        return path;
    }
    if (_dupenv_s(&buf, &len, "USERPROFILE") == 0 && buf != nullptr) {
        std::filesystem::path path = std::filesystem::path(buf) / ".ubaanext";
        free(buf);
        return path;
    }
#else
    if (const char *xdg_data_home = std::getenv("XDG_DATA_HOME")) {
        return std::filesystem::path(xdg_data_home) / "ubaanext";
    }
    if (const char *home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".ubaanext";
    }
#endif
    return ".ubaanext";
}

[[nodiscard]] std::filesystem::path get_session_file_path() {
    return get_app_data_dir() / "session.dat";
}

[[nodiscard]] std::filesystem::path get_cookie_file_path() {
    return get_app_data_dir() / "cookies.dat";
}

[[nodiscard]] std::filesystem::path get_config_file_path() {
    return get_app_data_dir() / "config.json";
}

// ── 上下文构建 ──────────────────────────────────────────────────

AppContext build_context(bool mock, const std::string &mode, const CliConfig &config) {
    AppContext ctx;
    ctx.mock_mode = mock;
#if UBAANEXT_ENABLE_MOCKS
    if (mock) {
        ctx.conn_mode = um::ConnectionMode::Mock;
    } else
#endif
    {
        ctx.conn_mode = (mode == "direct") ? um::ConnectionMode::Direct : um::ConnectionMode::WebVPN;
    }
    ctx.config = config;

#if UBAANEXT_ENABLE_MOCKS
    if (mock) {
        ctx.http = std::make_unique<UBAANextMocks::MockHttpClient>();
        ctx.cache = std::make_unique<UBAANextMocks::MockCacheStore>();
    } else
#endif
    {
#if defined(_WIN32)
        um::WinHttpConfig cfg;
        cfg.follow_redirects = false;
        if (!config.proxy.empty()) {
            cfg.proxy = config.proxy;
        }
        auto client = std::make_unique<um::WinHttpClient>(cfg);
        client->load_cookies(get_cookie_file_path().string());
        ctx.http = std::move(client);
#else
        ctx.http = nullptr;
#endif
        ctx.cache = std::make_unique<um::MemoryCacheStore>();
    }
#if defined(_WIN32)
    ctx.store = std::make_unique<UBAANextCli::PlainFileStore>(get_session_file_path(), true);
#else
    ctx.store = std::make_unique<UBAANextCli::PlainFileStore>(get_session_file_path(), false);
#endif
    return ctx;
}

// ── 帮助 ──────────────────────────────────────────────────

nlohmann::json get_help_json() {
    using json = nlohmann::json;
    json commands = json::array();

    auto add_cmd = [&](const std::string &name, const std::string &desc, const json &opts = {}) {
        json cmd = {{"name", name}, {"description", desc}};
        if (!opts.empty()) {
            cmd["options"] = opts;
        }
        commands.push_back(cmd);
    };

    add_cmd("version", "显示版本信息");
    add_cmd("help", "显示帮助信息");

    json login_opts = {
        {{"name", "--username"}, {"description", "学号"}, {"required", true}},
        {{"name", "--password"}, {"description", "密码"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("login", "登录", login_opts);

    add_cmd("whoami", "显示当前用户信息");
    add_cmd("logout", "登出");

    json course_opts = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("course today", "显示今天的课程", course_opts);

    json course_date_opts = {
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("course date", "显示指定日期的课程", course_date_opts);

    json course_week_opts = {
        {{"name", "--week"}, {"description", "周次 (1-30)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("course week", "显示指定周次的课程", course_week_opts);

    json exam_opts = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("exam list", "显示考试列表", exam_opts);

    json classroom_opts = {
        {{"name", "--campus"}, {"description", "校区 ID (1-10)"}, {"required", true}},
        {{"name", "--date"}, {"description", "日期 (yyyy-MM-dd)"}, {"required", true}},
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("classroom query", "查询空闲教室", classroom_opts);

    json term_opts = {
#if UBAANEXT_ENABLE_MOCKS
        {{"name", "--mock"}, {"description", "使用模拟数据"}, {"required", false}},
#endif
        {{"name", "--mode"}, {"description", "连接模式: vpn|direct"}, {"required", false}},
    };
    add_cmd("term list", "显示学期列表", term_opts);
    add_cmd("week list", "显示教学周列表", term_opts);

    add_cmd("config show", "显示当前配置");

    json config_set_opts = {
        {{"name", "--key"}, {"description", "配置键"}, {"required", true}},
        {{"name", "--value"}, {"description", "配置值"}, {"required", true}},
    };
    add_cmd("config set", "设置配置项", config_set_opts);

    add_cmd("cache clear", "清除缓存");
    add_cmd("user info", "显示用户信息");
    add_cmd("app version", "显示应用版本信息");
    add_cmd("app announcement", "显示公告");
    add_cmd("grade list", "显示指定学期成绩");
    add_cmd("grade all", "显示全部成绩");
    add_cmd("spoc assignments", "显示 SPOC 作业");
    add_cmd("spoc assignment show", "显示 SPOC 作业详情");
    add_cmd("judge assignments", "显示希冀作业");
    add_cmd("judge assignment show", "显示希冀作业概要");
    add_cmd("judge assignment details", "显示希冀作业详情");
    add_cmd("judge assignment details-batch", "批量显示希冀作业详情");
    add_cmd("signin today", "显示今日签到");
    add_cmd("signin do", "执行签到");
    add_cmd("bykc profile", "显示博雅资料");
    add_cmd("bykc courses", "显示博雅课程");
    add_cmd("bykc chosen", "显示已选博雅课程");
    add_cmd("bykc stats", "显示博雅统计");
    add_cmd("cgyy sites", "显示场馆列表");
    add_cmd("cgyy day-info", "显示场馆日期可预约信息");
    add_cmd("libbook libraries", "显示图书馆列表");
    add_cmd("libbook seats", "显示座位列表");
    add_cmd("ygdk overview", "显示阳光打卡概览");
    add_cmd("ygdk records", "显示阳光打卡记录");
    add_cmd("evaluation list", "显示评教任务");
    add_cmd("todo list", "显示待办聚合");

    return {{"ok", true}, {"data", {{"commands", commands}, {"version", UBAANEXT_VERSION_STRING}}}, {"error", nullptr}};
}

void print_usage() {
    UBAANextCli::Console::println("用法: ubaa <command> [options]\n");
    UBAANextCli::Console::println("命令:");
    UBAANextCli::Console::println("  version                          显示版本");
    UBAANextCli::Console::println("  help                             显示帮助");
    UBAANextCli::Console::println("  login --username <id> --password <pw>");
    UBAANextCli::Console::println("                                   登录（默认 VPN 模式）");
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  login --mock --username <id> --password <pw>");
    UBAANextCli::Console::println("                                   模拟登录");
#endif
    UBAANextCli::Console::println("  whoami                           显示当前用户");
    UBAANextCli::Console::println("  logout                           登出");
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  course today [--mock]            显示今天的课程");
#else
    UBAANextCli::Console::println("  course today                     显示今天的课程");
#endif
    UBAANextCli::Console::println("  course date --date <yyyy-MM-dd>  显示指定日期课程");
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  course week [--mock] --week <n>  显示指定周次课程");
#else
    UBAANextCli::Console::println("  course week --week <n>           显示指定周次课程");
#endif
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  exam list [--mock]               显示考试");
#else
    UBAANextCli::Console::println("  exam list                        显示考试");
#endif
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  classroom query [--mock] --campus <id> --date <yyyy-MM-dd>");
#else
    UBAANextCli::Console::println("  classroom query --campus <id> --date <yyyy-MM-dd>");
#endif
    UBAANextCli::Console::println("                                   查询空闲教室");
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  term list [--mock]               显示学期列表");
    UBAANextCli::Console::println("  week list [--mock]               显示教学周列表");
#else
    UBAANextCli::Console::println("  term list                        显示学期列表");
    UBAANextCli::Console::println("  week list                        显示教学周列表");
#endif
    UBAANextCli::Console::println("  config show                      显示当前配置");
    UBAANextCli::Console::println("  config set --key <key> --value <value>");
    UBAANextCli::Console::println("                                   设置配置项");
    UBAANextCli::Console::println("  cache clear                      清除缓存");
    UBAANextCli::Console::println("\n选项:");
    UBAANextCli::Console::println("  --json                           JSON 格式输出");
#if UBAANEXT_ENABLE_MOCKS
    UBAANextCli::Console::println("  --mock                           使用模拟数据");
#endif
    UBAANextCli::Console::println("  --mode vpn|direct                连接模式（默认 vpn）");
    UBAANextCli::Console::println("\n配置键:");
    UBAANextCli::Console::println("  mode      连接模式 (vpn|direct)");
    UBAANextCli::Console::println("  proxy     代理地址 (url 或空)");
    UBAANextCli::Console::println("  cache     缓存开关 (true|false)");
}

// ── 命令处理 ──────────────────────────────────────────────────

void save_real_cookies(ServiceFactory &factory);

ExitCode cmd_version(OutputFormatter &out) {
    out.print_version(UBAANEXT_VERSION_STRING);
    return ExitCode::Ok;
}

ExitCode cmd_help(OutputFormatter &out) {
    if (out.is_json()) {
        auto help_json = get_help_json();
        UBAANextCli::Console::println("{}", help_json.dump(2));
    } else {
        print_usage();
    }
    return ExitCode::Ok;
}

ExitCode cmd_login(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out, bool mock) {
    if (args.username.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "login 需要 --username <id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.password.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "login 需要 --password <pw>"});
        return ExitCode::InvalidArgument;
    }

    auto auth = factory.create_auth_service();

#if UBAANEXT_ENABLE_MOCKS
    if (mock) {
        auto result = auth.login_mock(args.username, args.password);
        if (!result) {
            out.print_error(result.error());
            return ExitCode::General;
        }
        out.print_login_result("登录成功（模拟）。", *result);
        return ExitCode::Ok;
    }
#else
    (void)mock;
#endif

    // 真实 CAS 登录
    um::ConnectionMode mode = um::ConnectionMode::WebVPN;  // 默认 VPN
    if (args.mode == "direct") {
        mode = um::ConnectionMode::Direct;
    }

    auto result = auth.login_real(args.username, args.password, mode);
    if (!result) {
        out.print_error(result.error());
        if (result.error().code == um::ErrorCode::AuthFailed) {
            return ExitCode::General;
        }
        return ExitCode::Network;
    }

    save_real_cookies(factory);

    out.print_login_result("登录成功。", *result);
    return ExitCode::Ok;
}

ExitCode cmd_whoami(ServiceFactory &factory, OutputFormatter &out) {
    auto auth = factory.create_auth_service();
    auto result = auth.restore_session();
    if (!result) {
        out.print_error({um::ErrorCode::SessionExpired, "未登录。请先使用 'ubaa login' 登录。"});
        return ExitCode::AuthRequired;
    }

    out.print_account(*result);
    return ExitCode::Ok;
}

ExitCode cmd_logout(ServiceFactory &factory, OutputFormatter &out) {
    auto auth = factory.create_auth_service();
    auto result = auth.logout();
    if (!result) {
        out.print_error(result.error());
        return ExitCode::General;
    }

    out.print_message("已登出。");
    return ExitCode::Ok;
}

ExitCode map_error_to_exit_code(const um::Error &error) {
    switch (error.code) {
    case um::ErrorCode::InvalidArgument: return ExitCode::InvalidArgument;
    case um::ErrorCode::SessionExpired:
    case um::ErrorCode::AuthFailed:      return ExitCode::AuthRequired;
    case um::ErrorCode::NetworkError:    return ExitCode::Network;
    case um::ErrorCode::ParseError:      return ExitCode::Parse;
    default:                             return ExitCode::General;
    }
}

void save_real_cookies(ServiceFactory &factory) {
#if defined(_WIN32)
    auto *win_http = dynamic_cast<um::WinHttpClient *>(&factory.http_client());
    if (win_http) {
        win_http->save_cookies(get_cookie_file_path().string());
    }
#else
    (void)factory;
#endif
}

ExitCode cmd_course_today(const CliArgs & /*args*/, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_course_service();
    auto result = service.get_today_courses();
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_courses(*result);
    return ExitCode::Ok;
}

ExitCode cmd_course_date(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.date.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "course date 需要 --date <yyyy-MM-dd>"});
        return ExitCode::InvalidArgument;
    }

    auto service = factory.create_course_service();
    auto result = service.get_date_courses(args.date);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_courses(*result);
    return ExitCode::Ok;
}

ExitCode cmd_course_week(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.week < 1) {
        out.print_error({um::ErrorCode::InvalidArgument, "course week 需要 --week <n> (1-30)"});
        return ExitCode::InvalidArgument;
    }

    std::string term_code;
    if (factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) {
        auto term_service = factory.create_term_service();
        auto terms = term_service.get_terms();
        if (!terms) {
            out.print_error(terms.error());
            return map_error_to_exit_code(terms.error());
        }
        for (const auto &term : *terms) {
            if (term.selected) {
                term_code = term.code;
                break;
            }
        }
        if (term_code.empty() && !terms->empty()) {
            term_code = terms->front().code;
        }
    }

    auto service = factory.create_course_service();
    auto result = term_code.empty()
        ? service.get_week_courses(args.week)
        : service.get_week_courses(args.week, term_code);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_courses(*result, args.week);
    return ExitCode::Ok;
}

ExitCode cmd_exam_list(const CliArgs & /*args*/, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_exam_service();
    auto result = service.get_exams();
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_exams(*result);
    return ExitCode::Ok;
}

ExitCode cmd_classroom_query(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.campus < 1) {
        out.print_error({um::ErrorCode::InvalidArgument, "classroom query 需要 --campus <id> (1-10)"});
        return ExitCode::InvalidArgument;
    }
    if (args.date.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "classroom query 需要 --date <yyyy-MM-dd>"});
        return ExitCode::InvalidArgument;
    }

    std::vector<int> sections;
    if (!args.sections.empty()) {
        auto parsed_sections = parse_sections_arg(args.sections);
        if (!parsed_sections) {
            out.print_error({um::ErrorCode::InvalidArgument, "--sections 格式无效，应为 1,2,3 形式且节次在 1-20 之间"});
            return ExitCode::InvalidArgument;
        }
        sections = *parsed_sections;
    }

    auto service = factory.create_classroom_service();
    auto result = args.username.empty() || args.password.empty()
        ? service.query_classrooms(args.campus, args.date, sections)
        : service.query_classrooms(args.campus, args.date, args.username, args.password, sections);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_classrooms(*result);
    return ExitCode::Ok;
}

ExitCode cmd_term_list(const CliArgs & /*args*/, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_term_service();
    auto result = service.get_terms();
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_terms(*result);
    return ExitCode::Ok;
}

ExitCode cmd_week_list(const CliArgs & /*args*/, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_term_service();
    std::string term_code;
    if (factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) {
        auto terms = service.get_terms();
        if (!terms) {
            out.print_error(terms.error());
            return map_error_to_exit_code(terms.error());
        }
        for (const auto &term : *terms) {
            if (term.selected) {
                term_code = term.code;
                break;
            }
        }
        if (term_code.empty() && !terms->empty()) {
            term_code = terms->front().code;
        }
    }

    auto result = service.get_weeks(term_code);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_weeks(*result);
    return ExitCode::Ok;
}

ExitCode cmd_config_show(OutputFormatter &out, const CliConfig &config) {
    if (out.is_json()) {
        nlohmann::json data = {
            {"mode",         config.mode},
            {"proxy",        config.proxy},
            {"cacheEnabled", config.cache_enabled},
            {"sessionPath",  get_session_file_path().string()},
            {"cookiePath",   get_cookie_file_path().string()},
            {"configPath",   get_config_file_path().string()},
            {"version",      UBAANEXT_VERSION_STRING},
        };
        nlohmann::json out_json = {{"ok", true}, {"data", data}, {"error", nullptr}};
        UBAANextCli::Console::println("{}", out_json.dump(2));
    } else {
        UBAANextCli::Console::println("当前配置:");
        UBAANextCli::Console::println("  模式:     {}", config.mode);
        UBAANextCli::Console::println("  代理:     {}", config.proxy.empty() ? "(无)" : config.proxy);
        UBAANextCli::Console::println("  缓存:     {}", config.cache_enabled ? "启用" : "禁用");
        UBAANextCli::Console::println("  会话文件: {}", get_session_file_path().string());
        UBAANextCli::Console::println("  Cookie:   {}", get_cookie_file_path().string());
        UBAANextCli::Console::println("  配置文件: {}", get_config_file_path().string());
        UBAANextCli::Console::println("  版本:     {}", UBAANEXT_VERSION_STRING);
    }
    return ExitCode::Ok;
}

ExitCode cmd_config_set(const CliArgs &args, OutputFormatter &out, CliConfig &config) {
    if (args.config_key.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "config set 需要 --key <key>"});
        return ExitCode::InvalidArgument;
    }
    if (args.config_value.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "config set 需要 --value <value>"});
        return ExitCode::InvalidArgument;
    }

    std::string key = args.config_key;
    std::string value = args.config_value;

    if (key == "mode") {
        if (value != "vpn" && value != "direct") {
            out.print_error({um::ErrorCode::InvalidArgument, "mode 值必须为 vpn 或 direct"});
            return ExitCode::InvalidArgument;
        }
        config.mode = value;
    } else if (key == "proxy") {
        config.proxy = value;
    } else if (key == "cache") {
        if (value == "true" || value == "1" || value == "yes") {
            config.cache_enabled = true;
        } else if (value == "false" || value == "0" || value == "no") {
            config.cache_enabled = false;
        } else {
            out.print_error({um::ErrorCode::InvalidArgument, "cache 值必须为 true 或 false"});
            return ExitCode::InvalidArgument;
        }
    } else {
        out.print_error({um::ErrorCode::InvalidArgument, "未知的配置键: " + key});
        return ExitCode::InvalidArgument;
    }

    // 保存配置
    auto config_path = get_config_file_path();
    std::filesystem::create_directories(config_path.parent_path());
    config.save(config_path.string());

    out.print_message(UBAANextCli::Console::format("配置已更新: {} = {}", key, value));
    return ExitCode::Ok;
}

ExitCode cmd_cache_clear(ServiceFactory & /*factory*/, OutputFormatter &out) {
    // v0.4 简化实现：mock 模式下缓存随进程销毁
    out.print_message("缓存已清除。");
    return ExitCode::Ok;
}

ExitCode cmd_grade_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_grade_service();
    auto result = args.all ? service.list_all_grades() : service.list_grades(args.term.empty() ? "2025-2026-2" : args.term);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_grades(*result);
    return ExitCode::Ok;
}

ExitCode cmd_feature_list(ServiceFactory &factory, OutputFormatter &out,
                          const std::string &domain, const std::string &operation,
                          const std::string &key) {
    auto service = factory.create_feature_service();
    auto result = service.list(domain, operation);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_records(key, *result);
    return ExitCode::Ok;
}

std::vector<std::string> parse_judge_batch_input(const std::string &input) {
    std::string text = input;
    if (!input.empty() && input.front() == '@') {
        std::ifstream file(input.substr(1));
        text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> ids;
    try {
        auto json = nlohmann::json::parse(text);
        if (json.is_array()) {
            for (const auto &item : json) {
                if (item.is_string()) ids.push_back(item.get<std::string>());
                else if (item.is_object() && item.contains("assignmentId")) ids.push_back(item["assignmentId"].get<std::string>());
            }
        } else if (json.is_object() && json.contains("assignmentIds") && json["assignmentIds"].is_array()) {
            for (const auto &item : json["assignmentIds"]) {
                if (item.is_string()) ids.push_back(item.get<std::string>());
            }
        }
    } catch (...) {
        std::string current;
        for (char ch : text) {
            if (ch == ',' || ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
                if (!current.empty()) {
                    ids.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) ids.push_back(current);
    }
    return ids;
}

ExitCode cmd_judge_details_batch(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.input.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "judge assignment details-batch 需要 --input <json|@file>"});
        return ExitCode::InvalidArgument;
    }
    auto ids = parse_judge_batch_input(args.input);
    if (ids.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "judge assignment details-batch 未解析到 assignment id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) {
        std::vector<um::Model::FeatureRecord> records;
        for (const auto &id : ids) {
            um::Model::FeatureRecord record;
            record.id = id;
            record.title = "评测任务";
            record.status = "unsubmitted";
            record.fields["source"] = "mock";
            record.fields["submissionStatus"] = "unsubmitted";
            records.push_back(std::move(record));
        }
        out.print_records("details", records);
        return ExitCode::Ok;
    }
#endif

    auto service = factory.create_judge_service();
    auto result = service.show_assignment_details_batch(ids);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_records("details", *result);
    return ExitCode::Ok;
}

ExitCode cmd_feature_show(ServiceFactory &factory, OutputFormatter &out,
                          const std::string &domain, const std::string &operation,
                          const std::string &id, const std::string &key) {
    auto service = factory.create_feature_service();
    auto result = service.show(domain, operation, id);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_record(key, *result);
    return ExitCode::Ok;
}

ExitCode cmd_feature_mutate(ServiceFactory &factory, OutputFormatter &out,
                            const std::string &domain, const std::string &operation,
                            const std::string &id, bool confirmed) {
    auto service = factory.create_feature_service();
    auto result = service.mutate(domain, operation, id, confirmed);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_mutation(*result);
    return ExitCode::Ok;
}

ExitCode cmd_user_info(ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_feature_service();
    auto result = service.user_info();
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_record("user", *result);
    return ExitCode::Ok;
}

bool is_pending_todo(const std::string &source, const um::Model::FeatureRecord &record) {
    if (source == "signin") return record.status == "available";
    if (source == "evaluation") return record.status == "pending";
    if (source == "judge") return record.status == "available" || record.status == "unsubmitted" || record.status == "partial";
    if (source == "spoc") return record.status == "open" || record.status == "available" || record.status == "pending";
    return record.status == "pending" || record.status == "available" || record.status == "open";
}

void normalize_todo_record(const std::string &source, um::Model::FeatureRecord &record) {
    record.fields["source"] = source;
    if (record.fields.find("type") == record.fields.end()) record.fields["type"] = source;
    if (record.fields.find("dueTime") == record.fields.end()) {
        auto deadline = record.fields.find("deadline");
        if (deadline != record.fields.end()) record.fields["dueTime"] = deadline->second;
    }
    if (record.fields.find("submissionStatus") == record.fields.end()) record.fields["submissionStatus"] = record.status;
}

void append_todo_records(std::vector<um::Model::FeatureRecord> &todos,
                         const std::string &source,
                         const std::vector<um::Model::FeatureRecord> &records,
                         bool pending_only) {
    for (auto record : records) {
        if (pending_only && !is_pending_todo(source, record)) continue;
        normalize_todo_record(source, record);
        todos.push_back(std::move(record));
    }
}

ExitCode cmd_todo_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_feature_service();
    std::vector<um::Model::FeatureRecord> todos;
    const bool pending_only = true;
    (void)args;

    if (auto spoc = service.list("spoc", "assignments")) append_todo_records(todos, "spoc", *spoc, pending_only);
    if (auto judge = service.list("judge", "assignments")) append_todo_records(todos, "judge", *judge, pending_only);
    if (auto signin = service.list("signin", "today")) append_todo_records(todos, "signin", *signin, pending_only);
    if (auto evaluation = service.list("evaluation", "list")) append_todo_records(todos, "evaluation", *evaluation, pending_only);

    save_real_cookies(factory);
    out.print_records("todos", todos);
    return ExitCode::Ok;
}

// ── 主入口 ──────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    CliArgs args = parse_args(argc, argv);

    OutputFormatter out(args.json_output);

    if (args.parse_error) {
        out.print_error({um::ErrorCode::InvalidArgument, args.error_message});
        return static_cast<int>(ExitCode::InvalidArgument);
    }


    if (args.command.empty() || args.command == "help") {
        return static_cast<int>(cmd_help(out));
    }

    if (args.command == "version") {
        return static_cast<int>(cmd_version(out));
    }

    // 加载配置
    CliConfig config;
    auto config_path = get_config_file_path();
    if (std::filesystem::exists(config_path)) {
        config = CliConfig::load(config_path.string());
    }

    // 命令行参数覆盖配置
    if (!args.mode.empty()) {
        config.mode = args.mode;
    }

    // 构建上下文
    auto ctx = build_context(args.mock, config.mode, config);
    ServiceFactory factory(ctx);

    if (args.command == "login") {
        return static_cast<int>(cmd_login(args, factory, out, args.mock));
    }
    if (args.command == "whoami") {
        return static_cast<int>(cmd_whoami(factory, out));
    }
    if (args.command == "logout") {
        return static_cast<int>(cmd_logout(factory, out));
    }

    if (args.command == "course") {
        if (args.subcommand == "today") return static_cast<int>(cmd_course_today(args, factory, out));
        if (args.subcommand == "date")  return static_cast<int>(cmd_course_date(args, factory, out));
        if (args.subcommand == "week")  return static_cast<int>(cmd_course_week(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 course 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "exam") {
        if (args.subcommand == "list") return static_cast<int>(cmd_exam_list(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 exam 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "classroom") {
        if (args.subcommand == "query") return static_cast<int>(cmd_classroom_query(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 classroom 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "term") {
        if (args.subcommand == "list") return static_cast<int>(cmd_term_list(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 term 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "week") {
        if (args.subcommand == "list") return static_cast<int>(cmd_week_list(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 week 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "config") {
        if (args.subcommand == "show") return static_cast<int>(cmd_config_show(out, config));
        if (args.subcommand == "set")  return static_cast<int>(cmd_config_set(args, out, config));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 config 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "cache") {
        if (args.subcommand == "clear") return static_cast<int>(cmd_cache_clear(factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 cache 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "user") {
        if (args.subcommand == "info") return static_cast<int>(cmd_user_info(factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 user 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "todo") {
        if (args.subcommand == "list") return static_cast<int>(cmd_todo_list(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 todo 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "app") {
        if (args.subcommand == "version") return static_cast<int>(cmd_feature_list(factory, out, "app", "version", "versions"));
        if (args.subcommand == "announcement") return static_cast<int>(cmd_feature_list(factory, out, "announcement", "list", "announcements"));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 app 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "grade") {
        if (args.subcommand == "list") return static_cast<int>(cmd_grade_list(args, factory, out));
        if (args.subcommand == "all") {
            args.all = true;
            return static_cast<int>(cmd_grade_list(args, factory, out));
        }
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 grade 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "spoc") {
        if (args.subcommand == "assignments") return static_cast<int>(cmd_feature_list(factory, out, "spoc", "assignments:" + std::string(args.pending_only ? "pending" : "all") + ":" + std::string(args.include_expired ? "include-expired" : "active"), "assignments"));
        if (args.subcommand == "assignment" && args.action == "show") return static_cast<int>(cmd_feature_show(factory, out, "spoc", "assignment", args.id, "assignment"));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 spoc 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "judge") {
        if (args.subcommand == "assignments") return static_cast<int>(cmd_feature_list(factory, out, "judge", "assignments:" + args.course_id + ":" + std::string(args.include_expired ? "include-expired" : "active") + ":" + std::string(args.include_history ? "include-history" : "current"), "assignments"));
        if (args.subcommand == "assignment" && args.action == "details-batch") return static_cast<int>(cmd_judge_details_batch(args, factory, out));
        if (args.subcommand == "assignment" && (args.action == "show" || args.action == "details")) return static_cast<int>(cmd_feature_show(factory, out, "judge", args.action, args.assignment_id.empty() ? args.id : args.assignment_id, args.action == "details" ? "details" : "assignment"));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 judge 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "signin") {
        if (args.subcommand == "today") return static_cast<int>(cmd_feature_list(factory, out, "signin", "today", "signin"));
        if (args.subcommand == "do") return static_cast<int>(cmd_feature_mutate(factory, out, "signin", "do", args.course_id.empty() ? args.id : args.course_id, args.confirmed));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 signin 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "ygdk") {
        if (args.subcommand == "overview") return static_cast<int>(cmd_feature_list(factory, out, "ygdk", "overview", "overview"));
        if (args.subcommand == "records") return static_cast<int>(cmd_feature_list(factory, out, "ygdk", "records:" + std::to_string(args.page) + ":" + std::to_string(args.size), "records"));
        if (args.subcommand == "submit") return static_cast<int>(cmd_feature_mutate(factory, out, "ygdk", "submit:" + args.start_time + "\n" + args.end_time + "\n" + args.place + "\n" + (args.share ? "1" : "0") + "\n" + args.photo_path, args.item_id.empty() ? args.id : args.item_id, args.confirmed));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 ygdk 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "evaluation") {
        if (args.subcommand == "list") return static_cast<int>(cmd_feature_list(factory, out, "evaluation", "list", "evaluations"));
        if (args.subcommand == "submit") return static_cast<int>(cmd_feature_mutate(factory, out, "evaluation", "submit", args.id, args.confirmed));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 evaluation 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "bykc") {
        if (args.subcommand == "profile") return static_cast<int>(cmd_feature_list(factory, out, "bykc", "profile", "profile"));
        if (args.subcommand == "courses") return static_cast<int>(cmd_feature_list(factory, out, "bykc", "courses:" + std::to_string(args.page) + ":" + std::to_string(args.size) + (args.all ? ":all" : ":page") + ":" + args.status + ":" + args.category + ":" + args.sub_category + ":" + (args.campus > 0 ? std::to_string(args.campus) : std::string{}) + ":" + args.keyword, "courses"));
        if (args.subcommand == "chosen") return static_cast<int>(cmd_feature_list(factory, out, "bykc", "chosen", "courses"));
        if (args.subcommand == "stats") return static_cast<int>(cmd_feature_list(factory, out, "bykc", "stats", "stats"));
        if (args.subcommand == "course" && args.action == "show") return static_cast<int>(cmd_feature_show(factory, out, "bykc", "course", args.course_id.empty() ? args.id : args.course_id, "course"));
        if (args.subcommand == "select" || args.subcommand == "unselect") return static_cast<int>(cmd_feature_mutate(factory, out, "bykc", args.subcommand, args.course_id.empty() ? args.id : args.course_id, args.confirmed));
        if (args.subcommand == "sign") {
            if (!args.has_lat || !args.has_lng || args.sign_type == 0) {
                out.print_error({um::ErrorCode::InvalidArgument, "bykc sign 需要 --course-id、--sign-type、--lat 和 --lng"});
                return static_cast<int>(ExitCode::InvalidArgument);
            }
            return static_cast<int>(cmd_feature_mutate(factory, out, "bykc", "sign:" + std::to_string(args.sign_type) + ":" + std::to_string(args.lat) + ":" + std::to_string(args.lng), args.course_id.empty() ? args.id : args.course_id, args.confirmed));
        }
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 bykc 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "cgyy") {
        if (args.subcommand == "sites") return static_cast<int>(cmd_feature_list(factory, out, "cgyy", "sites", "sites"));
        if (args.subcommand == "purpose-types") return static_cast<int>(cmd_feature_list(factory, out, "cgyy", "purpose-types", "purposeTypes"));
        if (args.subcommand == "day-info") return static_cast<int>(cmd_feature_list(factory, out, "cgyy", (args.date.empty() && args.site_id.empty() && args.id.empty()) ? "day-info" : "day-info:" + args.date + ":" + (args.site_id.empty() ? args.id : args.site_id), "dayInfo"));
        if (args.subcommand == "orders") return static_cast<int>(cmd_feature_list(factory, out, "cgyy", "orders:" + std::to_string(args.page) + ":" + std::to_string(args.size), "orders"));
        if (args.subcommand == "reserve") return static_cast<int>(cmd_feature_mutate(factory, out, "cgyy", "reserve:" + args.site_id + "\n" + args.space_id + "\n" + args.date + "\n" + args.purpose_type + "\n" + args.theme + "\n" + args.phone + "\n" + args.joiners + "\n" + args.captcha + "\n" + args.token, args.id, args.confirmed));
        if (args.subcommand == "order") {
            if (args.action == "cancel") return static_cast<int>(cmd_feature_mutate(factory, out, "cgyy", "cancel", args.order_id, args.confirmed));
            if (args.action == "show") return static_cast<int>(cmd_feature_show(factory, out, "cgyy", "show", args.order_id, "order"));
            if (args.action == "lock-code") return static_cast<int>(cmd_feature_show(factory, out, "cgyy", "lock-code", args.order_id.empty() ? "lock-code" : args.order_id, "order"));
        }
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 cgyy 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "libbook") {
        if (args.subcommand == "libraries") return static_cast<int>(cmd_feature_list(factory, out, "libbook", "libraries", "libraries"));
        if (args.subcommand == "areas") return static_cast<int>(cmd_feature_list(factory, out, "libbook", args.library_id.empty() ? "areas" : "areas:" + args.library_id + ":" + args.date + ":" + args.storey_id, "areas"));
        if (args.subcommand == "seats") return static_cast<int>(cmd_feature_list(factory, out, "libbook", args.area_id.empty() ? "seats" : "seats:" + args.area_id + ":" + args.date + ":" + args.start_time + ":" + args.end_time, "seats"));
        if (args.subcommand == "reservations") return static_cast<int>(cmd_feature_list(factory, out, "libbook", "reservations:" + std::to_string(args.page) + ":" + std::to_string(args.size), "reservations"));
        if (args.subcommand == "area" && args.action == "show") return static_cast<int>(cmd_feature_show(factory, out, "libbook", "area", args.area_id.empty() ? args.id : args.area_id, "area"));
        if (args.subcommand == "book") return static_cast<int>(cmd_feature_mutate(factory, out, "libbook", "book:" + args.date + "\n" + args.segment, args.seat_id.empty() ? args.id : args.seat_id, args.confirmed));
        if (args.subcommand == "cancel") return static_cast<int>(cmd_feature_mutate(factory, out, "libbook", "cancel", args.booking_id.empty() ? args.id : args.booking_id, args.confirmed));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 libbook 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    out.print_error({um::ErrorCode::InvalidArgument, "未知命令: " + args.command});
    return static_cast<int>(ExitCode::InvalidArgument);
}
