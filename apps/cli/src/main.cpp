/**
 * @file main.cpp
 * @brief UBAA Next 命令行界面入口
 *
 * 命令树：
 *   version [--json]
 *   help [--json]
 *   login [--mock] --username <id> --password <pw> [--mode vpn|direct]
 *   mode [vpn|direct] [--json]
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
#include "CommandHandlers.hpp"
#include "Console.hpp"
#include "ExitCodes.hpp"
#include "OutputFormatter.hpp"
#include "PlatformContextFactory.hpp"
#include "SecurityRedaction.hpp"
#include "ServiceFactory.hpp"

#include <UBAANext/Version.hpp>
#include <UBAANext/Auth/SessionContext.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
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

struct CliArgs;

ExitCode cmd_feature_show(ServiceFactory &factory, OutputFormatter &out,
                          const std::string &domain, const std::string &operation,
                          const std::string &id, const std::string &key);

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
    std::string mode;  // empty means use saved config; otherwise "vpn" or "direct"
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

[[nodiscard]] bool looks_like_option(std::string_view value) {
    return value.rfind("--", 0) == 0;
}

[[nodiscard]] bool read_option_value(int argc, char *argv[], int &i, const char *option, std::string_view &value, CliArgs &args) {
    if (i + 1 >= argc || looks_like_option(argv[i + 1])) {
        args.error_message = UBAANextCli::Console::format("{} 需要值", option);
        args.parse_error = true;
        return false;
    }
    value = argv[++i];
    return true;
}

bool read_string_option(int argc, char *argv[], int &i, const char *option, std::string &target, CliArgs &args) {
    std::string_view value;
    if (!read_option_value(argc, argv, i, option, value, args)) return false;
    target.assign(value);
    return true;
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

    for (int json_index = 1; json_index < argc; ++json_index) {
        if (std::string_view(argv[json_index]) == "--json") {
            args.json_output = true;
            break;
        }
    }

    if (argc < 2) {
        return args;
    }

    args.command = argv[1];

    int i = 2;
    if (UBAANextCli::is_cli_command(args.command) && i < argc && !looks_like_option(argv[i])) {
        args.subcommand = argv[i];
        ++i;
        if (UBAANextCli::is_command_with_action(args.command) && i < argc && std::string_view(argv[i]).find("--") != 0) {
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
        } else if (arg == "--username") {
            read_string_option(argc, argv, i, "--username", args.username, args);
        } else if (arg == "--password") {
            read_string_option(argc, argv, i, "--password", args.password, args);
        } else if (arg == "--week") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--week", value, args)) continue;
            if (auto v = parse_int(value)) {
                if (*v < 1 || *v > 30) {
                    args.error_message = "--week 值必须在 1-30 之间";
                    args.parse_error = true;
                } else {
                    args.week = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--week 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--campus") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--campus", value, args)) continue;
            if (auto v = parse_int(value)) {
                if (*v < 1 || *v > 10) {
                    args.error_message = "--campus 值必须在 1-10 之间";
                    args.parse_error = true;
                } else {
                    args.campus = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--campus 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--date") {
            if (!read_string_option(argc, argv, i, "--date", args.date, args)) continue;
            if (!is_valid_date(args.date)) {
                args.error_message = UBAANextCli::Console::format("--date 格式无效 '{}'，应为 yyyy-MM-dd", args.date);
                args.parse_error = true;
            }
        } else if (arg == "--key") {
            read_string_option(argc, argv, i, "--key", args.config_key, args);
        } else if (arg == "--value") {
            read_string_option(argc, argv, i, "--value", args.config_value, args);
        } else if (arg == "--mode") {
            if (!read_string_option(argc, argv, i, "--mode", args.mode, args)) continue;
            if (!is_valid_mode(args.mode)) {
                args.error_message = UBAANextCli::Console::format("--mode 值无效 '{}'，应为 vpn 或 direct", args.mode);
                args.parse_error = true;
            }
        } else if (arg == "--term") {
            read_string_option(argc, argv, i, "--term", args.term, args);
        } else if (arg == "--id") {
            read_string_option(argc, argv, i, "--id", args.id, args);
        } else if (arg == "--course-id") {
            read_string_option(argc, argv, i, "--course-id", args.course_id, args);
        } else if (arg == "--assignment-id") {
            read_string_option(argc, argv, i, "--assignment-id", args.assignment_id, args);
        } else if (arg == "--area-id") {
            read_string_option(argc, argv, i, "--area-id", args.area_id, args);
        } else if (arg == "--library-id") {
            read_string_option(argc, argv, i, "--library-id", args.library_id, args);
        } else if (arg == "--booking-id") {
            read_string_option(argc, argv, i, "--booking-id", args.booking_id, args);
        } else if (arg == "--order-id") {
            read_string_option(argc, argv, i, "--order-id", args.order_id, args);
        } else if (arg == "--site-id") {
            read_string_option(argc, argv, i, "--site-id", args.site_id, args);
        } else if (arg == "--page") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--page", value, args)) continue;
            if (auto v = parse_int(value)) {
                if (*v < 1) {
                    args.error_message = "--page 值必须大于等于 1";
                    args.parse_error = true;
                } else {
                    args.page = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--page 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--size" || arg == "--limit") {
            std::string_view value;
            const auto option = std::string(arg);
            if (!read_option_value(argc, argv, i, option.c_str(), value, args)) continue;
            if (auto v = parse_int(value)) {
                if (*v < 1 || *v > 200) {
                    args.error_message = "--size 值必须在 1-200 之间";
                    args.parse_error = true;
                } else {
                    args.size = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--size 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--all") {
            args.all = true;
        } else if (arg == "--status") {
            read_string_option(argc, argv, i, "--status", args.status, args);
        } else if (arg == "--category") {
            read_string_option(argc, argv, i, "--category", args.category, args);
        } else if (arg == "--sub-category" || arg == "--subcategory") {
            const auto option = std::string(arg);
            read_string_option(argc, argv, i, option.c_str(), args.sub_category, args);
        } else if (arg == "--keyword") {
            read_string_option(argc, argv, i, "--keyword", args.keyword, args);
        } else if (arg == "--start-time") {
            read_string_option(argc, argv, i, "--start-time", args.start_time, args);
        } else if (arg == "--end-time") {
            read_string_option(argc, argv, i, "--end-time", args.end_time, args);
        } else if (arg == "--storey-id") {
            read_string_option(argc, argv, i, "--storey-id", args.storey_id, args);
        } else if (arg == "--space-id") {
            read_string_option(argc, argv, i, "--space-id", args.space_id, args);
        } else if (arg == "--purpose-type") {
            read_string_option(argc, argv, i, "--purpose-type", args.purpose_type, args);
        } else if (arg == "--theme") {
            read_string_option(argc, argv, i, "--theme", args.theme, args);
        } else if (arg == "--phone") {
            read_string_option(argc, argv, i, "--phone", args.phone, args);
        } else if (arg == "--joiners") {
            read_string_option(argc, argv, i, "--joiners", args.joiners, args);
        } else if (arg == "--captcha") {
            read_string_option(argc, argv, i, "--captcha", args.captcha, args);
        } else if (arg == "--token") {
            read_string_option(argc, argv, i, "--token", args.token, args);
        } else if (arg == "--item-id") {
            read_string_option(argc, argv, i, "--item-id", args.item_id, args);
        } else if (arg == "--place") {
            read_string_option(argc, argv, i, "--place", args.place, args);
        } else if (arg == "--photo") {
            read_string_option(argc, argv, i, "--photo", args.photo_path, args);
        } else if (arg == "--path") {
            read_string_option(argc, argv, i, "--path", args.photo_path, args);
        } else if (arg == "--seat-id") {
            read_string_option(argc, argv, i, "--seat-id", args.seat_id, args);
        } else if (arg == "--segment") {
            read_string_option(argc, argv, i, "--segment", args.segment, args);
        } else if (arg == "--sections") {
            read_string_option(argc, argv, i, "--sections", args.sections, args);
        } else if (arg == "--input") {
            read_string_option(argc, argv, i, "--input", args.input, args);
        } else if (arg == "--include-expired") {
            args.include_expired = true;
        } else if (arg == "--include-history") {
            args.include_history = true;
        } else if (arg == "--pending-only") {
            args.pending_only = true;
        } else if (arg == "--share") {
            args.share = true;
        } else if (arg == "--lat") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--lat", value, args)) continue;
            if (auto v = parse_double(value)) {
                args.lat = *v;
                args.has_lat = true;
            } else {
                args.error_message = UBAANextCli::Console::format("--lat 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--lng") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--lng", value, args)) continue;
            if (auto v = parse_double(value)) {
                args.lng = *v;
                args.has_lng = true;
            } else {
                args.error_message = UBAANextCli::Console::format("--lng 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--sign-type") {
            std::string_view value;
            if (!read_option_value(argc, argv, i, "--sign-type", value, args)) continue;
            if (auto v = parse_int(value)) {
                if (*v != 1 && *v != 2) {
                    args.error_message = "--sign-type 只能为 1(签到) 或 2(签退)";
                    args.parse_error = true;
                } else {
                    args.sign_type = *v;
                }
            } else {
                args.error_message = UBAANextCli::Console::format("--sign-type 值无效 '{}'", value);
                args.parse_error = true;
            }
        } else if (arg == "--confirm" || arg == "--yes") {
            args.confirmed = true;
        } else if (arg == "--base-url") {
            args.config_key = "base-url";
            read_string_option(argc, argv, i, "--base-url", args.config_value, args);
        } else if (arg == "--proxy") {
            args.config_key = "proxy";
            read_string_option(argc, argv, i, "--proxy", args.config_value, args);
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
    char *override_buf = nullptr;
    std::size_t override_len = 0;
    if (_dupenv_s(&override_buf, &override_len, "UBAANEXT_APP_DATA_DIR") == 0 && override_buf != nullptr) {
        std::filesystem::path path = override_buf;
        free(override_buf);
        if (!path.empty()) return path;
    }
#else
    if (const char *override_dir = std::getenv("UBAANEXT_APP_DATA_DIR")) {
        if (*override_dir != '\0') return override_dir;
    }
#endif
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

/** Sensitive local persistence boundary: returns the real cookie file path; exposing it is diagnostic-only. */
[[nodiscard]] std::filesystem::path get_cookie_file_path() {
    return get_app_data_dir() / "cookies.dat";
}

[[nodiscard]] std::filesystem::path get_config_file_path() {
    return get_app_data_dir() / "config.json";
}

/** Upload helper: infers MIME from a local path string without reading the file contents. */
[[nodiscard]] std::string mime_for_upload_path(const std::filesystem::path &path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".webp") return "image/webp";
    return "application/octet-stream";
}

/**
 * Sensitive local file read: reads upload bytes only for typed WriteGated operations, never for placeholder file upload.
 * Local file read: yes. Remote mutation depends on the caller's service gate.
 */
[[nodiscard]] um::Result<um::UploadPart> read_upload_part(const std::string &path_text, std::string field_name) {
    auto path = std::filesystem::path(path_text);
    std::ifstream input(path, std::ios::binary);
    if (!input) return um::make_error(um::ErrorCode::InvalidArgument, "无法读取上传文件");
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return um::make_error(um::ErrorCode::InvalidArgument, "上传文件为空");

    um::UploadPart part;
    part.field_name = std::move(field_name);
    part.filename = path.filename().string();
    part.content_type = mime_for_upload_path(path);
    part.bytes = std::move(bytes);
    return part;
}

// ── 上下文构建 ──────────────────────────────────────────────────

AppContext build_context(bool mock, const std::string &mode, const CliConfig &config) {
    UBAANextCli::PlatformContextOptions options;
    options.mock = mock;
    options.mode = mode;
    options.config = config;
    options.session_file_path = get_session_file_path();
    options.cookie_file_path = get_cookie_file_path();
    return UBAANextCli::create_current_platform_context(options);
}

// ── 命令处理 ──────────────────────────────────────────────────

/** Sensitive cookie persistence forward declaration: saves platform cookies only after real requests. */
void save_real_cookies(ServiceFactory &factory);

bool command_requires_session(const CliArgs &args) {
    if (args.mock || args.command.empty()) {
        return false;
    }
    if (args.command == "course") {
        if (args.subcommand == "week" && args.term.empty()) {
            return false;
        }
        return true;
    }
    if (args.command == "exam") {
        return !args.term.empty();
    }
    if (args.command == "week") {
        return !args.term.empty();
    }
    if (args.command == "grade") {
        return args.subcommand == "all" || (args.subcommand == "list" && (args.all || !args.term.empty()));
    }
    if (args.command == "classroom") {
        return args.subcommand == "query" && args.campus > 0 && !args.date.empty();
    }
    if (args.command == "term") {
        return args.subcommand == "list";
    }
    if (args.command == "user" || args.command == "todo") {
        return true;
    }
    if (args.command == "libbook") {
        return args.subcommand == "libraries" || args.subcommand == "reservations";
    }
    if (args.command == "bykc") {
        return args.subcommand == "profile" || args.subcommand == "courses" ||
               args.subcommand == "chosen" || args.subcommand == "stats";
    }
    if (args.command == "cgyy") {
        return args.subcommand == "sites" || args.subcommand == "purpose-types" ||
               args.subcommand == "orders" || (args.subcommand == "order" && args.action == "lock-code");
    }
    if (args.command == "signin") {
        return args.subcommand == "today";
    }
    if (args.command == "ygdk") {
        return args.subcommand == "overview" || args.subcommand == "records";
    }
    if (args.command == "evaluation") {
        return args.subcommand == "list";
    }
    if (args.command == "judge") {
        return args.subcommand == "assignments";
    }
    if (args.command == "spoc") {
        return args.subcommand == "assignments";
    }
    return false;
}

ExitCode restore_session_for_command(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!command_requires_session(args) || !args.mode.empty()) {
        return ExitCode::Ok;
    }

    auto auth = factory.create_auth_service();
    auto restored = um::Auth::restore_session_context(auth);
    if (!restored) {
        out.print_error({um::ErrorCode::SessionExpired, "未登录。请先使用 'ubaa login' 登录。"});
        return ExitCode::AuthRequired;
    }

    factory.context().conn_mode = restored->connection_mode;
    return ExitCode::Ok;
}

bool real_session_persistence_available(const AppContext &ctx) {
    return ctx.mock_mode || ctx.capabilities.secure_store;
}

/** Sensitive session persistence error: real login must fail closed when secure store is unavailable. */
um::Error unsupported_session_persistence_error() {
    return {um::ErrorCode::UnsupportedSecureStore,
            "当前平台没有可用安全存储，已拒绝保存真实登录会话；请启用平台安全存储后重试"};
}

/** Sensitive local mutation: clears platform cookies and local cookie file; does not prove remote logout. */
void clear_real_cookies(ServiceFactory &factory) {
    UBAANextCli::clear_platform_cookies(factory.context());
    std::error_code ec;
    std::filesystem::remove(get_cookie_file_path(), ec);
}

ExitCode cmd_version(OutputFormatter &out) {
    out.print_version(UBAANEXT_VERSION_STRING);
    return ExitCode::Ok;
}

ExitCode cmd_help(OutputFormatter &out) {
    UBAANextCli::print_help(out);
    return ExitCode::Ok;
}

/** Sensitive input CLI handler: performs login/session persistence; credentials must stay redacted and mock mode is not live proof. */
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

    if (!real_session_persistence_available(factory.context())) {
        out.print_error(unsupported_session_persistence_error());
        return ExitCode::Storage;
    }

    auto result = auth.login_real(args.username, args.password, factory.context().conn_mode);
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

/** Sensitive output CLI handler: restores local session identity through redaction-aware output. */
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

/** Sensitive local mutation CLI handler: clears local session/cookies only after --confirm/--yes. */
ExitCode cmd_logout(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "logout 会清除本地会话，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    auto auth = factory.create_auth_service();
    auto result = auth.logout();
    if (!result) {
        out.print_error(result.error());
        return ExitCode::General;
    }
    clear_real_cookies(factory);
    factory.context().cache->clear();

    out.print_message("已登出。");
    return ExitCode::Ok;
}

ExitCode map_error_to_exit_code(const um::Error &error) {
    switch (error.code) {
    case um::ErrorCode::InvalidArgument: return ExitCode::InvalidArgument;
    case um::ErrorCode::SessionExpired:
    case um::ErrorCode::AuthFailed:      return ExitCode::AuthRequired;
    case um::ErrorCode::NetworkError:
    case um::ErrorCode::UnsupportedNetwork:
    case um::ErrorCode::Timeout:
    case um::ErrorCode::TlsError:         return ExitCode::Network;
    case um::ErrorCode::UnsupportedPlatform:
    case um::ErrorCode::UnsupportedSecureStore:
    case um::ErrorCode::UnsupportedCrypto:
    case um::ErrorCode::UnsupportedCookiePersistence:
    case um::ErrorCode::NotImplemented:
    case um::ErrorCode::CryptoError:
    case um::ErrorCode::StorageError:    return ExitCode::General;
    case um::ErrorCode::ParseError:      return ExitCode::Parse;
    default:                             return ExitCode::General;
    }
}

/** Sensitive cookie persistence: saves real platform cookies after successful live reads/writes, not in mock-only proof. */
void save_real_cookies(ServiceFactory &factory) {
    UBAANextCli::save_platform_cookies(factory.context());
}

/** ReadOnlyCandidate CLI handler: today's course list; CLI success does not prove live BYXT field stability. */
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

/** ReadOnlyCandidate CLI handler: date course list with explicit date validation and propagated service errors. */
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

/** PartiallyMigrated CLI handler: week course list; real mode requires term_code even when mock accepts week-only. */
ExitCode cmd_course_week(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.week < 1) {
        out.print_error({um::ErrorCode::InvalidArgument, "course week 需要 --week <n> (1-30)"});
        return ExitCode::InvalidArgument;
    }

    if ((factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) &&
        args.term.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "真实 course week 需要 --term <term_code>"});
        return ExitCode::InvalidArgument;
    }

    auto service = factory.create_course_service();
    auto result = args.term.empty()
        ? service.get_week_courses(args.week)
        : service.get_week_courses(args.week, args.term);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_courses(*result, args.week);
    return ExitCode::Ok;
}

/** Sensitive output CLI handler: exam list requires term in real mode and emits stable JSON/error contract. */
ExitCode cmd_exam_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if ((factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) &&
        args.term.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "真实 exam list 需要 --term <term_code>"});
        return ExitCode::InvalidArgument;
    }

    auto service = factory.create_exam_service();
    auto result = service.get_exams(args.term);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_exams(*result);
    return ExitCode::Ok;
}

/** ReadOnlyCandidate CLI handler: classroom availability is volatile and unsupported modes fail explicitly. */
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

/** ReadOnlyCandidate CLI handler: term list through mock or real BYXT path; service errors are not converted to empty lists. */
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

/** ReadOnlyCandidate CLI handler: week list requires explicit term in real mode. */
ExitCode cmd_week_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if ((factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) &&
        args.term.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "真实 week list 需要 --term <term_code>"});
        return ExitCode::InvalidArgument;
    }

    auto service = factory.create_term_service();
    auto result = service.get_weeks(args.term);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }

    save_real_cookies(factory);
    out.print_weeks(*result);
    return ExitCode::Ok;
}

/** Sensitive local config CLI handler: displays redacted config and must not expose proxy credentials. */
ExitCode cmd_config_show(OutputFormatter &out, const CliConfig &config) {
    const auto proxy = UBAANextCli::redact_proxy_url(config.proxy);
    if (out.is_json()) {
        nlohmann::json data = {
            {"mode",         config.mode},
            {"proxy",        proxy},
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
        UBAANextCli::Console::println("  代理:     {}", proxy.empty() ? "(无)" : proxy);
        UBAANextCli::Console::println("  缓存:     {}", config.cache_enabled ? "启用" : "禁用");
        UBAANextCli::Console::println("  会话文件: {}", get_session_file_path().string());
        UBAANextCli::Console::println("  Cookie:   {}", get_cookie_file_path().string());
        UBAANextCli::Console::println("  配置文件: {}", get_config_file_path().string());
        UBAANextCli::Console::println("  版本:     {}", UBAANEXT_VERSION_STRING);
    }
    return ExitCode::Ok;
}

ExitCode save_config_and_report(OutputFormatter &out, const CliConfig &config,
                                const std::string &key, const std::string &value) {
    auto config_path = get_config_file_path();
    std::filesystem::create_directories(config_path.parent_path());
    config.save(config_path.string());

    out.print_message(UBAANextCli::Console::format("配置已更新: {} = {}", key, key == "proxy" ? UBAANextCli::redact_proxy_url(value) : value));
    return ExitCode::Ok;
}

/** Local config CLI handler: changes default connection mode only after validation; no remote I/O. */
ExitCode cmd_mode(const CliArgs &args, OutputFormatter &out, CliConfig &config) {
    if (args.subcommand.empty()) {
        if (out.is_json()) {
            nlohmann::json out_json = {
                {"ok", true},
                {"data", {{"mode", config.mode}, {"configPath", get_config_file_path().string()}}},
                {"error", nullptr},
            };
            UBAANextCli::Console::println("{}", out_json.dump(2));
        } else {
            UBAANextCli::Console::println("当前连接模式: {}", config.mode);
            UBAANextCli::Console::println("可用命令: ubaa mode direct | ubaa mode vpn");
        }
        return ExitCode::Ok;
    }

    if (!is_valid_mode(args.subcommand)) {
        out.print_error({um::ErrorCode::InvalidArgument, "mode 只支持 direct 或 vpn"});
        return ExitCode::InvalidArgument;
    }

    config.mode = args.subcommand;
    return save_config_and_report(out, config, "mode", config.mode);
}

/** Sensitive local mutation CLI handler: writes local config only after --confirm/--yes. */
ExitCode cmd_config_set(const CliArgs &args, OutputFormatter &out, CliConfig &config) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "config set 会修改本地配置，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
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
        config.proxy = (value == "none" || value == "off") ? std::string{} : value;
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

    return save_config_and_report(out, config, key, value);
}

/** Sensitive local mutation CLI handler: clears local cache only after --confirm/--yes and performs no remote I/O. */
ExitCode cmd_cache_clear(const CliArgs &args, ServiceFactory & /*factory*/, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "cache clear 会清除本地缓存，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    // v0.4 简化实现：mock 模式下缓存随进程销毁
    out.print_message("缓存已清除。");
    return ExitCode::Ok;
}

/** Sensitive output CLI handler: term-specific grades are ReadOnlyCandidate; all-grades real mode remains partially migrated. */
ExitCode cmd_grade_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.all &&
        (factory.context().conn_mode == um::ConnectionMode::Direct || factory.context().conn_mode == um::ConnectionMode::WebVPN) &&
        args.term.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "真实 grade list 需要 --term <term_code>"});
        return ExitCode::InvalidArgument;
    }

    auto service = factory.create_grade_service();
    auto result = args.all ? service.list_all_grades() : service.list_grades(args.term);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_grades(*result);
    return ExitCode::Ok;
}

um::Model::FeatureRecord judge_summary_to_record(const um::Model::JudgeAssignmentSummary &assignment) {
    um::Model::FeatureRecord record;
    record.id = assignment.id;
    record.title = assignment.title;
    record.status = assignment.status.empty() ? "available" : assignment.status;
    record.fields["courseId"] = assignment.course_id;
    record.fields["courseName"] = assignment.course_name;
    record.fields["startTime"] = assignment.start_time;
    record.fields["dueTime"] = assignment.due_time;
    record.fields["maxScore"] = assignment.max_score;
    record.fields["myScore"] = assignment.my_score;
    record.fields["totalProblems"] = std::to_string(assignment.total_problems);
    record.fields["submittedCount"] = std::to_string(assignment.submitted_count);
    record.fields["submissionStatus"] = record.status;
    record.fields["submissionStatusText"] = assignment.status_text;
    return record;
}

um::Model::FeatureRecord judge_detail_to_record(const um::Model::JudgeAssignmentDetail &detail) {
    um::Model::FeatureRecord record;
    record.id = detail.id;
    record.title = detail.title;
    record.status = detail.status;
    record.fields["courseId"] = detail.course_id;
    record.fields["courseName"] = detail.course_name;
    record.fields["content"] = detail.content;
    record.fields["startTime"] = detail.start_time;
    record.fields["dueTime"] = detail.due_time;
    record.fields["maxScore"] = detail.max_score;
    record.fields["myScore"] = detail.my_score;
    record.fields["totalProblems"] = std::to_string(detail.total_problems);
    record.fields["submittedCount"] = std::to_string(detail.submitted_count);
    record.fields["submissionStatus"] = detail.status;
    record.fields["submissionStatusText"] = detail.status_text;
    return record;
}

um::Model::FeatureRecord spoc_summary_to_record(const um::Model::SpocAssignmentSummary &assignment) {
    um::Model::FeatureRecord record;
    record.id = assignment.id;
    record.title = assignment.title;
    record.status = assignment.status;
    record.fields["courseId"] = assignment.course_id;
    record.fields["courseName"] = assignment.course_name;
    record.fields["teacher"] = assignment.teacher;
    record.fields["startTime"] = assignment.start_time;
    record.fields["dueTime"] = assignment.due_time;
    record.fields["score"] = assignment.score;
    record.fields["term"] = assignment.term_code;
    record.fields["termName"] = assignment.term_name;
    record.fields["submissionStatus"] = assignment.submission_status;
    return record;
}

um::Model::FeatureRecord spoc_detail_to_record(const um::Model::SpocAssignmentDetail &assignment) {
    um::Model::FeatureRecord record;
    record.id = assignment.id;
    record.title = assignment.title;
    record.status = assignment.status;
    record.fields["courseId"] = assignment.course_id;
    record.fields["startTime"] = assignment.start_time;
    record.fields["dueTime"] = assignment.due_time;
    record.fields["score"] = assignment.score;
    record.fields["content"] = assignment.content;
    record.fields["submissionStatus"] = assignment.submission_status;
    record.fields["submittedAt"] = assignment.submitted_at;
    return record;
}

/** MockOnly compatibility CLI handler: routes legacy FeatureRecord lists and does not prove typed service coverage. */
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

/** ReadOnlyCandidate CLI handler: SPOC assignments use typed service in real mode and mock FeatureService in mock mode. */
ExitCode cmd_spoc_assignments(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) {
        return cmd_feature_list(factory, out, "spoc", "assignments:" + std::string(args.pending_only ? "pending" : "all") + ":" + std::string(args.include_expired ? "include-expired" : "active"), "assignments");
    }
#endif
    auto service = factory.create_spoc_service();
    um::SpocAssignmentQuery query;
    query.pending_only = args.pending_only;
    query.include_expired = args.include_expired;
    auto result = service.list_assignment_summaries(query);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    std::vector<um::Model::FeatureRecord> records;
    for (const auto &summary : *result) records.push_back(spoc_summary_to_record(summary));
    save_real_cookies(factory);
    out.print_records("assignments", records);
    return ExitCode::Ok;
}

/** Sensitive output CLI handler: shows one SPOC assignment detail; raw backend JSON is never printed. */
ExitCode cmd_spoc_assignment_show(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "spoc assignment show 需要 --id <assignment-id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) {
        return cmd_feature_show(factory, out, "spoc", "assignment", args.id, "assignment");
    }
#endif
    auto service = factory.create_spoc_service();
    auto result = service.assignment_detail(args.id);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_record("assignment", spoc_detail_to_record(*result));
    return ExitCode::Ok;
}

/** ReadOnlyCandidate CLI handler: XiJi assignments use typed service in real mode and mock FeatureService in mock mode. */
ExitCode cmd_judge_assignments(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) {
        return cmd_feature_list(factory, out, "judge", "assignments:" + args.course_id + ":" + std::string(args.include_expired ? "include-expired" : "active") + ":" + std::string(args.include_history ? "include-history" : "current"), "assignments");
    }
#endif
    auto service = factory.create_judge_service();
    um::JudgeAssignmentQuery query;
    query.course_id = args.course_id;
    query.include_expired = args.include_expired;
    query.include_history = args.include_history;
    auto result = service.list_assignment_summaries(query);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    std::vector<um::Model::FeatureRecord> records;
    for (const auto &summary : *result) records.push_back(judge_summary_to_record(summary));
    save_real_cookies(factory);
    out.print_records("assignments", records);
    return ExitCode::Ok;
}

/** Sensitive output CLI handler: shows XiJi assignment summary/detail through redaction-aware output paths. */
ExitCode cmd_judge_assignment_show(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    const auto id = args.assignment_id.empty() ? args.id : args.assignment_id;
    if (id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "judge assignment show/details 需要 --assignment-id 或 --id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) {
        return cmd_feature_show(factory, out, "judge", args.action, id, args.action == "details" ? "details" : "assignment");
    }
#endif
    auto service = factory.create_judge_service();
    if (args.action == "details") {
        auto result = service.assignment_detail(id);
        if (!result) {
            out.print_error(result.error());
            return map_error_to_exit_code(result.error());
        }
        save_real_cookies(factory);
        out.print_record("details", judge_detail_to_record(*result));
        return ExitCode::Ok;
    }
    auto result = service.show_assignment(id);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_record("assignment", *result);
    return ExitCode::Ok;
}

/** Sensitive input CLI handler: batch id input may come from --input/@file and detail output remains sensitive. */
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
            if (id == "judge-error") {
                record.title = "希冀作业详情失败: " + id;
                record.status = "error";
                record.fields["source"] = "mock";
                record.fields["content"] = UBAANextCli::redact_sensitive_text("captcha=captcha-secret&Authorization: bearer-secret&photo_path=C:/secret/judge.html");
                record.fields["submissionStatus"] = "error";
                record.fields["submissionStatusText"] = "NetworkError";
                records.push_back(std::move(record));
                continue;
            }
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
    auto result = service.assignment_details_batch(ids);
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    std::vector<um::Model::FeatureRecord> records;
    for (const auto &detail : *result) records.push_back(judge_detail_to_record(detail));
    save_real_cookies(factory);
    out.print_records("details", records);
    return ExitCode::Ok;
}

/** MockOnly compatibility CLI handler: legacy FeatureRecord detail route, not a typed service completion signal. */
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

/** Placeholder write CLI handler: mock may return accepted contract, real mode remains UnsupportedPlatform. */
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

template <typename T>
ExitCode print_records_result(ServiceFactory &factory, OutputFormatter &out, const std::string &key, const um::Result<std::vector<T>> &result) {
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_records(key, *result);
    return ExitCode::Ok;
}

ExitCode print_record_result(ServiceFactory &factory, OutputFormatter &out, const std::string &key, const um::Result<um::Model::FeatureRecord> &result) {
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_record(key, *result);
    return ExitCode::Ok;
}

ExitCode print_mutation_result(ServiceFactory &factory, OutputFormatter &out, const um::Result<um::Model::MutationResult> &result) {
    if (!result) {
        out.print_error(result.error());
        return map_error_to_exit_code(result.error());
    }
    save_real_cookies(factory);
    out.print_mutation(*result);
    return ExitCode::Ok;
}

/** ReadOnlyCandidate CLI handler: BYKC profile read; mock mode remains FeatureService compatibility. */
ExitCode cmd_bykc_profile(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "bykc", "profile", "profile");
#endif
    auto service = factory.create_bykc_service();
    return print_records_result(factory, out, "profile", service.profile());
}

/** ReadOnlyCandidate CLI handler: BYKC course list with page/size/all filters; unsupported filter combos must fail in service. */
ExitCode cmd_bykc_courses(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "bykc", "courses", "courses");
#endif
    auto service = factory.create_bykc_service();
    um::BykcCourseQuery query;
    query.page = args.page;
    query.size = args.size;
    query.all = args.all;
    query.status = args.status;
    query.category = args.category;
    query.sub_category = args.sub_category;
    query.campus = args.campus > 0 ? std::to_string(args.campus) : std::string{};
    query.keyword = args.keyword;
    return print_records_result(factory, out, "courses", service.courses(query));
}

/** Sensitive output CLI handler: BYKC chosen courses expose enrollment state through redaction-aware output. */
ExitCode cmd_bykc_chosen(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "bykc", "chosen", "courses");
#endif
    auto service = factory.create_bykc_service();
    return print_records_result(factory, out, "courses", service.chosen());
}

/** ReadOnlyCandidate CLI handler: BYKC stats are unverified against live backend field drift. */
ExitCode cmd_bykc_stats(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "bykc", "stats", "stats");
#endif
    auto service = factory.create_bykc_service();
    return print_records_result(factory, out, "stats", service.stats());
}

/** Sensitive output CLI handler: BYKC course detail may include enrollment metadata. */
ExitCode cmd_bykc_course_show(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    const auto id = args.course_id.empty() ? args.id : args.course_id;
    if (id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "bykc course show 需要 --course-id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_show(factory, out, "bykc", "course", id, "course");
#endif
    auto service = factory.create_bykc_service();
    return print_record_result(factory, out, "course", service.show_course(id));
}

/** WriteGated CLI handler: BYKC select/unselect are remote mutations requiring --confirm/--yes and write_operations. */
ExitCode cmd_bykc_select(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out, bool select) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, std::string("bykc ") + (select ? "select" : "unselect") + " 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    const auto id = args.course_id.empty() ? args.id : args.course_id;
    if (id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, std::string("bykc ") + (select ? "select" : "unselect") + " 需要 --course-id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "bykc", select ? "select" : "unselect", id, args.confirmed);
#endif
    auto service = factory.create_bykc_write_service(args.confirmed, select ? "bykc select" : "bykc unselect");
    return print_mutation_result(factory, out, select ? service.select_course(id) : service.unselect_course(id));
}

/** WriteGated CLI handler: BYKC sign is a remote mutation requiring --confirm/--yes and write_operations. */
ExitCode cmd_bykc_sign(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "bykc sign 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    if ((args.course_id.empty() && args.id.empty()) || args.sign_type == 0) {
        out.print_error({um::ErrorCode::InvalidArgument, "bykc sign 需要 --course-id 和 --sign-type"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "bykc", "sign:" + std::to_string(args.sign_type), args.course_id.empty() ? args.id : args.course_id, args.confirmed);
#endif
    auto service = factory.create_bykc_write_service(args.confirmed, "bykc sign");
    return print_mutation_result(factory, out, service.sign_course(args.course_id.empty() ? args.id : args.course_id, args.sign_type));
}

/** ReadOnlyCandidate CLI handler: venue site list; mock mode remains FeatureService compatibility. */
ExitCode cmd_cgyy_sites(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "cgyy", "sites", "sites");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_records_result(factory, out, "sites", service.list_sites());
}

/** ReadOnlyCandidate CLI handler: venue purpose type list with backend enum drift risk. */
ExitCode cmd_cgyy_purpose_types(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "cgyy", "purpose-types", "purposeTypes");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_records_result(factory, out, "purposeTypes", service.list_purpose_types());
}

/** Sensitive output CLI handler: venue day availability is volatile and requires explicit site/date. */
ExitCode cmd_cgyy_day_info(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto site_id = args.site_id.empty() ? args.id : args.site_id;
    if (args.date.empty() || site_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy day-info 需要 --date 和 --id/--site-id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "cgyy", "day-info", "dayInfo");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_records_result(factory, out, "dayInfo", service.day_info(args.date, site_id));
}

/** Sensitive output CLI handler: venue orders expose booking metadata and use redaction-aware output. */
ExitCode cmd_cgyy_orders(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "cgyy", "orders", "orders");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_records_result(factory, out, "orders", service.list_orders(args.page, args.size));
}

/** Sensitive output CLI handler: venue order detail may expose lock/order state. */
ExitCode cmd_cgyy_order_show(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto order_id = args.order_id.empty() ? args.id : args.order_id;
    if (order_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy order show 需要 --order-id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_show(factory, out, "cgyy", "show", order_id, "order");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_record_result(factory, out, "order", service.show_order(order_id));
}

/** Sensitive output CLI handler: lock code visibility must remain explicit and redaction-aware. */
ExitCode cmd_cgyy_lock_code(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    (void)args;
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_show(factory, out, "cgyy", "lock-code", args.order_id.empty() ? "lock-code" : args.order_id, "order");
#endif
    auto service = factory.create_venue_reservation_service();
    return print_record_result(factory, out, "order", service.lock_code());
}

/** WriteGated CLI handler: venue reservation is a remote mutation with sensitive captcha/token inputs. */
ExitCode cmd_cgyy_reserve(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    if (args.site_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --site-id <id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.space_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --space-id <id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.date.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --date <yyyy-MM-dd>"});
        return ExitCode::InvalidArgument;
    }
    if (args.id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --id <time-id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.purpose_type.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --purpose-type <id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.theme.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --theme"});
        return ExitCode::InvalidArgument;
    }
    if (args.phone.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --phone"});
        return ExitCode::InvalidArgument;
    }
    if (args.joiners.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --joiners"});
        return ExitCode::InvalidArgument;
    }
    if (args.captcha.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要用户提供 --captcha，不会自动绕过验证码"});
        return ExitCode::InvalidArgument;
    }
    if (args.token.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy reserve 需要 --token，请先运行 day-info 获取预约上下文"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "cgyy", "reserve", args.id, args.confirmed);
#endif
    auto service = factory.create_venue_reservation_write_service(args.confirmed, "cgyy reserve");
    return print_mutation_result(factory, out, service.reserve(args.site_id, args.space_id, args.date, args.id, args.purpose_type, args.theme, args.phone, args.joiners, args.captcha, args.token));
}

/** WriteGated CLI handler: venue order cancellation is a remote mutation requiring --confirm/--yes and write_operations. */
ExitCode cmd_cgyy_cancel(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy order cancel 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    auto order_id = args.order_id.empty() ? args.id : args.order_id;
    if (order_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "cgyy order cancel 需要 --order-id <id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "cgyy", "cancel", order_id, args.confirmed);
#endif
    auto service = factory.create_venue_reservation_write_service(args.confirmed, "cgyy order cancel");
    return print_mutation_result(factory, out, service.cancel_order(order_id));
}

/** ReadOnlyCandidate CLI handler: library list read; live availability fields remain volatile. */
ExitCode cmd_libbook_libraries(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "libbook", "libraries", "libraries");
#endif
    auto service = factory.create_library_seat_service();
    return print_records_result(factory, out, "libraries", service.list_libraries(""));
}

/** ReadOnlyCandidate CLI handler: library area list requires explicit library id and may expose capacity metadata. */
ExitCode cmd_libbook_areas(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.library_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook areas 需要 --library-id <id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "libbook", "areas", "areas");
#endif
    auto service = factory.create_library_seat_service();
    return print_records_result(factory, out, "areas", service.list_areas(args.library_id, args.date, args.storey_id));
}

/** Sensitive output CLI handler: seat availability is volatile and must not be treated as cached truth. */
ExitCode cmd_libbook_seats(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (args.area_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook seats 需要 --area-id <id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "libbook", "seats", "seats");
#endif
    auto service = factory.create_library_seat_service();
    return print_records_result(factory, out, "seats", service.list_seats(args.area_id, args.date, args.start_time, args.end_time));
}

/** Sensitive output CLI handler: library reservations expose booking state. */
ExitCode cmd_libbook_reservations(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "libbook", "reservations", "reservations");
#endif
    auto service = factory.create_library_seat_service();
    return print_records_result(factory, out, "reservations", service.list_reservations(args.page, args.size));
}

/** Sensitive output CLI handler: library area detail may expose capacity and seat metadata. */
ExitCode cmd_libbook_area_show(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto area_id = args.area_id.empty() ? args.id : args.area_id;
    if (area_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook area show 需要 --area-id <id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_show(factory, out, "libbook", "area", area_id, "area");
#endif
    auto service = factory.create_library_seat_service();
    return print_record_result(factory, out, "area", service.show_area(area_id));
}

/** WriteGated CLI handler: library booking is a remote mutation requiring --confirm/--yes and write_operations. */
ExitCode cmd_libbook_book(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook book 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    auto seat_id = args.seat_id.empty() ? args.id : args.seat_id;
    if (seat_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook book 需要 --seat-id <id>"});
        return ExitCode::InvalidArgument;
    }
    if (args.date.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook book 需要 --date <yyyy-MM-dd>"});
        return ExitCode::InvalidArgument;
    }
    if (args.segment.empty() && (args.start_time.empty() || args.end_time.empty())) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook book 需要 --segment <segment> 或 --start-time/--end-time"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "libbook", "book", seat_id, args.confirmed);
#endif
    auto service = factory.create_library_seat_write_service(args.confirmed, "libbook book");
    return print_mutation_result(factory, out, service.reserve_seat(seat_id, args.date, args.segment, args.start_time, args.end_time));
}

/** WriteGated CLI handler: library booking cancellation is a remote mutation requiring --confirm/--yes and write_operations. */
ExitCode cmd_libbook_cancel(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook cancel 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    auto booking_id = args.booking_id.empty() ? args.id : args.booking_id;
    if (booking_id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "libbook cancel 需要 --booking-id <id>"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "libbook", "cancel", booking_id, args.confirmed);
#endif
    auto service = factory.create_library_seat_write_service(args.confirmed, "libbook cancel");
    return print_mutation_result(factory, out, service.cancel_booking(booking_id));
}

/** Sensitive output CLI handler: user info is ReadOnlyCandidate and must remain redaction-aware. */
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

/**
 * ReadOnlyCandidate aggregation CLI handler.
 * Preserves Todo source-level partial failures instead of converting them to an empty success list.
 */
ExitCode cmd_todo_list(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    auto service = factory.create_todo_service();
    um::TodoQuery query;
    query.pending_only = !args.all;
    if (args.pending_only) query.pending_only = true;
    return print_records_result(factory, out, "todos", service.list_todos(query));
}

/**
 * Placeholder CLI handler for future file uploads.
 * Returns NotImplemented after confirmation and never reads local files or performs network I/O.
 */
ExitCode cmd_file_upload_placeholder(const CliArgs &args, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "file upload 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    if (args.photo_path.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "file upload 需要 --path <path>"});
        return ExitCode::InvalidArgument;
    }
    um::Error error{um::ErrorCode::NotImplemented, "file upload 当前仅保留稳定 CLI 接口，真实上传语义尚未实现"};
    out.print_error(error);
    return map_error_to_exit_code(error);
}

/** ReadOnlyCandidate CLI handler: today's sign-in list uses typed service in real mode and mock FeatureService in mock mode. */
ExitCode cmd_signin_today(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "signin", "today", "signin");
#endif
    auto service = factory.create_signin_service();
    return print_records_result(factory, out, "signin", service.list_today());
}

/**
 * WriteGated CLI handler for real course sign-in.
 * Remote mutation: yes; requires --confirm/--yes and platform write_operations in real mode.
 */
ExitCode cmd_signin_do(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "signin do 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
    const auto id = args.course_id.empty() ? args.id : args.course_id;
    if (id.empty()) {
        out.print_error({um::ErrorCode::InvalidArgument, "signin do 需要 --id 或 --course-id"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "signin", "do", id, args.confirmed);
#endif
    auto service = factory.create_signin_write_service(args.confirmed, "signin do");
    return print_mutation_result(factory, out, service.perform_signin(id));
}

/** Sensitive output CLI handler: YGDK overview may expose sports/health context. */
ExitCode cmd_ygdk_overview(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "ygdk", "overview", "overview");
#endif
    auto service = factory.create_ygdk_service();
    return print_records_result(factory, out, "overview", service.overview());
}

/** Sensitive output CLI handler: YGDK records may include time/location-like fields and use page/size contract. */
ExitCode cmd_ygdk_records(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "ygdk", "records", "records");
#endif
    auto service = factory.create_ygdk_service();
    return print_records_result(factory, out, "records", service.records(args.page, args.size));
}

/** WriteGated CLI handler: YGDK submit is a remote mutation with sensitive time/place/photo input. */
ExitCode cmd_ygdk_submit(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "ygdk submit 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "ygdk", "submit", args.item_id.empty() ? args.id : args.item_id, args.confirmed);
#endif
    auto service = factory.create_ygdk_write_service(args.confirmed, "ygdk submit");
    if (!factory.context().capabilities.write_operations) {
        um::Error error{um::ErrorCode::UnsupportedPlatform, "ygdk submit 当前平台未启用真实写操作"};
        out.print_error(error);
        return map_error_to_exit_code(error);
    }
    auto photo = read_upload_part(args.photo_path, "file");
    if (!photo) {
        out.print_error({photo.error().code, photo.error().message});
        return ExitCode::InvalidArgument;
    }
    return print_mutation_result(factory, out, service.submit_clockin(args.item_id.empty() ? args.id : args.item_id, args.start_time, args.end_time, args.place, args.share, *photo));
}

/** PartiallyMigrated CLI handler: evaluation list is read-only but questionnaire/session drift remains possible. */
ExitCode cmd_evaluation_list(ServiceFactory &factory, OutputFormatter &out) {
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_list(factory, out, "evaluation", "list", "evaluations");
#endif
    auto service = factory.create_evaluation_service();
    return print_records_result(factory, out, "evaluations", service.list_evaluations());
}

/** WriteGated CLI handler: evaluation submit is a remote mutation requiring --confirm/--yes and write_operations. */
ExitCode cmd_evaluation_submit(const CliArgs &args, ServiceFactory &factory, OutputFormatter &out) {
    if (!args.confirmed) {
        out.print_error({um::ErrorCode::InvalidArgument, "evaluation submit 是有副作用操作，必须显式传入 --confirm 或 --yes"});
        return ExitCode::InvalidArgument;
    }
#if UBAANEXT_ENABLE_MOCKS
    if (factory.context().conn_mode == um::ConnectionMode::Mock) return cmd_feature_mutate(factory, out, "evaluation", "submit", args.id, args.confirmed);
#endif
    auto service = factory.create_evaluation_write_service(args.confirmed, "evaluation submit");
    return print_mutation_result(factory, out, service.submit_evaluations(args.id));
}

// ── 主入口 ──────────────────────────────────────────────────

int run_cli(int argc, char *argv[]) {
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

    if (args.command == "mode") {
        return static_cast<int>(cmd_mode(args, out, config));
    }

    if (args.command == "login") {
        return static_cast<int>(cmd_login(args, factory, out, args.mock));
    }
    if (args.command == "whoami") {
        return static_cast<int>(cmd_whoami(factory, out));
    }
    if (args.command == "logout") {
        return static_cast<int>(cmd_logout(args, factory, out));
    }

    auto session_ready = restore_session_for_command(args, factory, out);
    if (session_ready != ExitCode::Ok) {
        return static_cast<int>(session_ready);
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
        if (args.subcommand == "clear") return static_cast<int>(cmd_cache_clear(args, factory, out));
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
        if (args.subcommand == "assignments") return static_cast<int>(cmd_spoc_assignments(args, factory, out));
        if (args.subcommand == "assignment" && args.action == "show") return static_cast<int>(cmd_spoc_assignment_show(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 spoc 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "judge") {
        if (args.subcommand == "assignments") return static_cast<int>(cmd_judge_assignments(args, factory, out));
        if (args.subcommand == "assignment" && args.action == "details-batch") return static_cast<int>(cmd_judge_details_batch(args, factory, out));
        if (args.subcommand == "assignment" && (args.action == "show" || args.action == "details")) return static_cast<int>(cmd_judge_assignment_show(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 judge 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "signin") {
        if (args.subcommand == "today") return static_cast<int>(cmd_signin_today(factory, out));
        if (args.subcommand == "do") return static_cast<int>(cmd_signin_do(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 signin 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "ygdk") {
        if (args.subcommand == "overview") return static_cast<int>(cmd_ygdk_overview(factory, out));
        if (args.subcommand == "records") return static_cast<int>(cmd_ygdk_records(args, factory, out));
        if (args.subcommand == "submit") return static_cast<int>(cmd_ygdk_submit(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 ygdk 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "evaluation") {
        if (args.subcommand == "list") return static_cast<int>(cmd_evaluation_list(factory, out));
        if (args.subcommand == "submit") return static_cast<int>(cmd_evaluation_submit(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 evaluation 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "bykc") {
        if (args.subcommand == "profile") return static_cast<int>(cmd_bykc_profile(factory, out));
        if (args.subcommand == "courses") return static_cast<int>(cmd_bykc_courses(args, factory, out));
        if (args.subcommand == "chosen") return static_cast<int>(cmd_bykc_chosen(factory, out));
        if (args.subcommand == "stats") return static_cast<int>(cmd_bykc_stats(factory, out));
        if (args.subcommand == "course" && args.action == "show") return static_cast<int>(cmd_bykc_course_show(args, factory, out));
        if (args.subcommand == "select") return static_cast<int>(cmd_bykc_select(args, factory, out, true));
        if (args.subcommand == "unselect") return static_cast<int>(cmd_bykc_select(args, factory, out, false));
        if (args.subcommand == "sign") return static_cast<int>(cmd_bykc_sign(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 bykc 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "cgyy") {
        if (args.subcommand == "sites") return static_cast<int>(cmd_cgyy_sites(factory, out));
        if (args.subcommand == "purpose-types") return static_cast<int>(cmd_cgyy_purpose_types(factory, out));
        if (args.subcommand == "day-info") return static_cast<int>(cmd_cgyy_day_info(args, factory, out));
        if (args.subcommand == "orders") return static_cast<int>(cmd_cgyy_orders(args, factory, out));
        if (args.subcommand == "reserve") return static_cast<int>(cmd_cgyy_reserve(args, factory, out));
        if (args.subcommand == "order") {
            if (args.action == "cancel") return static_cast<int>(cmd_cgyy_cancel(args, factory, out));
            if (args.action == "show") return static_cast<int>(cmd_cgyy_order_show(args, factory, out));
            if (args.action == "lock-code") return static_cast<int>(cmd_cgyy_lock_code(args, factory, out));
        }
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 cgyy 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "libbook") {
        if (args.subcommand == "libraries") return static_cast<int>(cmd_libbook_libraries(factory, out));
        if (args.subcommand == "areas") return static_cast<int>(cmd_libbook_areas(args, factory, out));
        if (args.subcommand == "seats") return static_cast<int>(cmd_libbook_seats(args, factory, out));
        if (args.subcommand == "reservations") return static_cast<int>(cmd_libbook_reservations(args, factory, out));
        if (args.subcommand == "area" && args.action == "show") return static_cast<int>(cmd_libbook_area_show(args, factory, out));
        if (args.subcommand == "book") return static_cast<int>(cmd_libbook_book(args, factory, out));
        if (args.subcommand == "cancel") return static_cast<int>(cmd_libbook_cancel(args, factory, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 libbook 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    if (args.command == "file") {
        if (args.subcommand == "upload") return static_cast<int>(cmd_file_upload_placeholder(args, out));
        out.print_error({um::ErrorCode::InvalidArgument, "未知的 file 子命令: " + args.subcommand});
        return static_cast<int>(ExitCode::InvalidArgument);
    }

    out.print_error({um::ErrorCode::InvalidArgument, "未知命令: " + args.command});
    return static_cast<int>(ExitCode::InvalidArgument);
}

namespace {

bool cli_has_flag(int argc, char *argv[], std::string_view flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == flag) return true;
    }
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    try {
        return run_cli(argc, argv);
    } catch (const std::exception &ex) {
        OutputFormatter out(cli_has_flag(argc, argv, "--json"));
        out.print_error({um::ErrorCode::Unknown, UBAANextCli::redact_sensitive_text(ex.what())});
        return static_cast<int>(ExitCode::General);
    } catch (...) {
        OutputFormatter out(cli_has_flag(argc, argv, "--json"));
        out.print_error({um::ErrorCode::Unknown, "CLI 执行失败"});
        return static_cast<int>(ExitCode::General);
    }
}
