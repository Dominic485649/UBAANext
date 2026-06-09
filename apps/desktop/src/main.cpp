#include <UBAANext/Runtime/AppRuntime.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Version.hpp>

#include "CliRunner.hpp"
#include "SecurityRedaction.hpp"
#include "main_window.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <cstdio>
#include <io.h>
#include <windows.h>
#endif

namespace runtime = UBAANext::Runtime;

namespace {

#if defined(_WIN32)
void attach_parent_console() {
    const auto stdout_handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
    if (stdout_handle != INVALID_HANDLE_VALUE && GetFileType(stdout_handle) == FILE_TYPE_PIPE) return;
    if (AttachConsole(ATTACH_PARENT_PROCESS) == 0) return;
    FILE *stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
}

void write_stdout(std::string_view text) {
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || GetFileType(handle) == FILE_TYPE_UNKNOWN) {
        attach_parent_console();
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN) {
        DWORD written = 0;
        WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }
    std::cout << text;
}

void write_stderr(std::string_view text) {
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || GetFileType(handle) == FILE_TYPE_UNKNOWN) {
        attach_parent_console();
        handle = GetStdHandle(STD_ERROR_HANDLE);
    }
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE && GetFileType(handle) != FILE_TYPE_UNKNOWN) {
        DWORD written = 0;
        WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }
    std::cerr << text;
}
#else
void attach_parent_console() {}

void write_stdout(std::string_view text) {
    std::cout << text;
}

void write_stderr(std::string_view text) {
    std::cerr << text;
}
#endif

bool has_arg(int argc, char *argv[], const std::string &needle) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == needle) return true;
    }
    return false;
}

bool has_argument(const std::vector<std::string> &arguments, std::string_view needle) {
    return std::find(arguments.begin(), arguments.end(), needle) != arguments.end();
}

bool is_space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

std::vector<std::string> split_command_line(std::string_view text) {
    std::vector<std::string> arguments;
    std::string current;
    bool in_quotes = false;
    char quote = '\0';
    bool has_token = false;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (in_quotes) {
            if (ch == quote) {
                in_quotes = false;
                has_token = true;
                continue;
            }
            if (ch == '\\' && i + 1 < text.size() && (text[i + 1] == quote || text[i + 1] == '\\')) {
                current.push_back(text[++i]);
                has_token = true;
                continue;
            }
            current.push_back(ch);
            has_token = true;
            continue;
        }
        if (ch == '\'' || ch == '"') {
            in_quotes = true;
            quote = ch;
            has_token = true;
            continue;
        }
        if (is_space(ch)) {
            if (has_token) {
                arguments.push_back(current);
                current.clear();
                has_token = false;
            }
            continue;
        }
        current.push_back(ch);
        has_token = true;
    }

    if (has_token) arguments.push_back(current);
    return arguments;
}

std::string format_command_result(const UBAANextCli::CliCommandResult &result) {
    std::ostringstream out;
    out << "exit code: " << result.exit_code << "\n";
    out << "stdout:\n" << UBAANextCli::redact_sensitive_text(result.stdout_text) << "\n";
    out << "stderr:\n" << UBAANextCli::redact_sensitive_text(result.stderr_text);
    return UBAANext::Security::redact_sensitive_text(out.str());
}

std::shared_ptr<slint::VectorModel<slint::SharedString>> string_model(const std::vector<std::string> &rows) {
    std::vector<slint::SharedString> values;
    values.reserve(rows.size());
    for (const auto &row : rows) values.emplace_back(row);
    return std::make_shared<slint::VectorModel<slint::SharedString>>(std::move(values));
}

std::string mount_summary(const runtime::CloudMountManager &mounts) {
    auto statuses = mounts.statuses();
    if (statuses.empty()) return "Mounts: stopped";
    std::string text = "Mounts:";
    for (const auto &status : statuses) {
        text += " " + runtime::CloudMountManager::frontend_name(status.frontend);
        text += status.running ? (status.writable ? "(rw)" : "(ro)") : "(off)";
    }
    return text;
}

std::string yes_no(bool value) {
    return value ? "yes" : "no";
}

std::string format_bytes(std::uintmax_t bytes) {
    const char *units[] = {"B", "KiB", "MiB", "GiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(units)) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream out;
    out << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << ' ' << units[unit];
    return out.str();
}

std::string account_summary_from_diag(const runtime::RuntimeDiagnostics &diag) {
    if (!diag.session_present) return "Session: not loaded";
    return "Session: " + UBAANext::Security::redact_sensitive_text(diag.account_summary);
}

std::string capabilities_summary(const runtime::RuntimeDiagnostics &diag) {
    return "Capabilities: secure_store=" + yes_no(diag.secure_store) +
           " cookies=" + yes_no(diag.cookie_persistence) +
           " live_login=" + yes_no(diag.live_login) +
           " writes=" + yes_no(diag.write_operations) +
           " WinFsp=" + yes_no(diag.winfsp_available) +
           " CloudFiles=" + yes_no(diag.cloud_files_available) +
           " FUSE=" + yes_no(diag.fuse_available);
}

std::vector<std::string> rows_from_mounts(const runtime::CloudMountManager &mounts) {
    std::vector<std::string> rows;
    for (const auto &status : mounts.statuses()) {
        rows.push_back(runtime::CloudMountManager::frontend_name(status.frontend) + " " +
                       (status.running ? "running" : "stopped") + " " +
                       (status.writable ? "rw" : "ro") + " mount=" +
                       UBAANext::Security::redact_sensitive_text(status.mount_point.string()) + " cache=" +
                       UBAANext::Security::redact_sensitive_text(status.cache_dir.string()) + " " +
                       UBAANext::Security::redact_sensitive_text(status.message));
    }
    if (rows.empty()) rows.push_back("No mount frontends started");
    return rows;
}

std::vector<std::string> rows_from_tasks(UBAANext::CloudVfs::CloudVfs &vfs) {
    std::vector<std::string> rows;
    for (const auto &task : vfs.tasks()) {
        rows.push_back("#" + std::to_string(task.id) + " " + task.name + " " + task.error_code + " " +
                       UBAANext::Security::redact_sensitive_text(task.error_message));
    }
    if (rows.empty()) rows.push_back("No active tasks");
    return rows;
}

std::vector<std::string> rows_from_cloud_state(const runtime::CloudBrowserState &state) {
    std::vector<std::string> rows;
    for (const auto &item : state.items) {
        rows.push_back(std::string(item.is_dir ? "[dir] " : "[file] ") + item.path + "  " + item.docid + "  " +
                       format_bytes(item.size));
    }
    if (rows.empty()) rows.push_back(state.status.empty() ? "Cloud directory is empty" : state.status);
    return rows;
}

std::vector<std::string> rows_from_cloud_tasks(const runtime::CloudBrowserState &state) {
    std::vector<std::string> rows;
    for (const auto &task : state.tasks) {
        rows.push_back("#" + std::to_string(task.id) + " " + task.operation + " " + task.status + " " +
                       std::to_string(task.progress) + "% " + task.path + " " +
                       UBAANext::Security::redact_sensitive_text(task.message));
    }
    if (rows.empty()) rows.push_back("No active cloud tasks");
    return rows;
}

std::string campus_operation(std::string domain, std::string operation, std::string id) {
    if (domain == "course") {
        if (operation == "today" || id.empty()) return operation;
        if (operation == "week") return "week:" + id;
        if (operation == "date") return "date:" + id;
    }
    if (domain == "exam") return id.empty() ? "list" : "list:" + id;
    if (domain == "grade") return id.empty() ? "list" : id;
    if (domain == "classroom") return id.empty() ? "query" : "query:" + id;
    if (domain == "spoc" && operation == "assignments") return id.empty() ? operation : operation + ":" + id;
    if (domain == "judge" && operation == "assignments") return id.empty() ? operation : operation + ":" + id;
    if (domain == "todo") return id.empty() ? "list" : id;
    return id.empty() ? operation : operation + ":" + id;
}

std::optional<runtime::RuntimeFeatureQuery> campus_detail_query(const std::string &text, bool confirmed) {
    auto first = text.find(':');
    auto second = first == std::string::npos ? std::string::npos : text.find(':', first + 1);
    if (first == std::string::npos || second == std::string::npos) return std::nullopt;
    runtime::RuntimeFeatureQuery query;
    query.domain = text.substr(0, first);
    query.operation = text.substr(first + 1, second - first - 1);
    query.id = text.substr(second + 1);
    query.confirmed = confirmed;
    return query;
}

std::optional<int> parse_positive_int(const std::string &text) {
    try {
        std::size_t parsed = 0;
        const int value = std::stoi(text, &parsed);
        if (parsed != text.size() || value <= 0) return std::nullopt;
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> split_colon_fields(const std::string &text) {
    std::vector<std::string> fields;
    std::string current;
    std::istringstream input(text);
    while (std::getline(input, current, ':')) fields.push_back(current);
    return fields;
}

std::vector<int> parse_sections(const std::string &text) {
    std::vector<int> sections;
    std::string item;
    std::istringstream input(text);
    while (std::getline(input, item, ',')) {
        auto section = parse_positive_int(item);
        if (section) sections.push_back(*section);
    }
    return sections;
}

UBAANext::Result<runtime::RuntimeFeatureState> run_campus_feature(runtime::RuntimeContext &ctx,
                                                                 const std::string &domain,
                                                                 const std::string &operation,
                                                                 const std::string &id,
                                                                 bool confirmed) {
    if (domain == "course" && operation == "today") return ctx.today_courses();
    if (domain == "course" && operation == "date") return ctx.date_courses(id);
    if (domain == "course" && operation == "week") {
        const auto split = id.find(':');
        const auto week_text = split == std::string::npos ? id : id.substr(0, split);
        const auto week = parse_positive_int(week_text);
        if (!week) return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "week must be a positive integer");
        const auto term = split == std::string::npos ? std::string{} : id.substr(split + 1);
        return ctx.week_courses(*week, term);
    }
    if (domain == "exam" && operation == "list") return ctx.exams(id);
    if (domain == "grade" && operation == "list") return ctx.grades(id, id.empty());
    if (domain == "classroom" && operation == "query") {
        auto fields = split_colon_fields(id);
        if (fields.size() < 2) return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "classroom format must be campus:date[:sections]");
        auto campus = parse_positive_int(fields[0]);
        if (!campus) return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "campus id must be a positive integer");
        return ctx.classrooms(*campus, fields[1], fields.size() >= 3 ? parse_sections(fields[2]) : std::vector<int>{});
    }
    if (domain == "spoc" && operation == "assignments") return ctx.spoc_assignments(id == "pending");
    if (domain == "judge" && operation == "assignments") return ctx.judge_assignments(id);
    if (domain == "todo" && operation == "list") return ctx.todos(id != "all");
    if (domain == "bykc" && operation == "courses") return ctx.bykc_courses();
    if (domain == "cgyy" && operation == "sites") return ctx.venue_sites();
    if (domain == "libbook" && operation == "libraries") return ctx.library_libraries(id);
    if (domain == "ygdk" && operation == "overview") return ctx.ygdk_overview();
    if (domain == "evaluation" && operation == "list") return ctx.evaluations();
    if (domain == "user" && operation == "info") return ctx.user_info_state();
    if (domain == "detail" && operation == "show") {
        auto query = campus_detail_query(id, false);
        if (!query) return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "detail format must be domain:operation:id");
        return ctx.feature_show(*query);
    }
    if (domain == "mutation" && operation == "run") {
        auto query = campus_detail_query(id, confirmed);
        if (!query) return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "mutation format must be domain:operation:id");
        return ctx.feature_mutate(*query);
    }

    runtime::RuntimeFeatureQuery query;
    query.domain = domain;
    query.operation = campus_operation(domain, operation, id);
    query.confirmed = confirmed;
    return ctx.feature_list(query);
}

std::vector<std::string> rows_from_feature_state(const runtime::RuntimeFeatureState &state) {
    std::vector<std::string> rows;
    for (const auto &row : state.rows) {
        std::string text = row.title;
        if (!row.status.empty()) text += " [" + row.status + "]";
        if (!row.id.empty()) text += " id=" + row.id;
        if (!row.subtitle.empty()) text += " " + row.subtitle;
        rows.push_back(UBAANext::Security::redact_sensitive_text(text));
        for (const auto &detail : row.details) {
            rows.push_back(UBAANext::Security::redact_sensitive_text("  " + detail));
        }
    }
    if (rows.empty()) rows.push_back(state.status.empty() ? "No campus records" : state.status);
    return rows;
}

void apply_feature_state(const slint::ComponentHandle<MainWindow> &ui, const runtime::RuntimeFeatureState &state) {
    ui->set_campus_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(state.status.empty() ? state.title : state.status)));
    ui->set_campus_rows(string_model(rows_from_feature_state(state)));
}

void apply_feature_error(const slint::ComponentHandle<MainWindow> &ui, const UBAANext::Error &error) {
    ui->set_campus_status(slint::SharedString("Campus error: " + UBAANext::Security::redact_sensitive_text(error.message)));
}

void apply_cloud_state(const slint::ComponentHandle<MainWindow> &ui, const runtime::CloudBrowserState &state) {
    ui->set_cloud_status(slint::SharedString(state.status));
    ui->set_cloud_breadcrumb(slint::SharedString(state.breadcrumb));
    ui->set_cloud_rows(string_model(rows_from_cloud_state(state)));
    ui->set_task_rows(string_model(rows_from_cloud_tasks(state)));
}

void apply_cloud_error(const slint::ComponentHandle<MainWindow> &ui, const UBAANext::Error &error) {
    ui->set_cloud_status(slint::SharedString("Cloud error: " + UBAANext::Security::redact_sensitive_text(error.message)));
}

void refresh_runtime_state(const slint::ComponentHandle<MainWindow> &ui, runtime::RuntimeContext &ctx) {
    auto diag = ctx.diagnostics();
    ui->set_version(slint::SharedString("v" + diag.version));
    ui->set_mode(slint::SharedString(diag.mode));
    ui->set_account_summary(slint::SharedString(account_summary_from_diag(diag)));
    ui->set_session_status(slint::SharedString(diag.session_present ? "Session loaded" : "No local session"));
    ui->set_capabilities_status(slint::SharedString(capabilities_summary(diag)));
    ui->set_cache_status(slint::SharedString("Cache: " + UBAANext::Security::redact_sensitive_text(diag.cache_dir.string()) +
                                             " (" + format_bytes(diag.cache_size_bytes) + ")"));
    apply_cloud_state(ui, ctx.cloud_state());
    ui->set_mount_status(slint::SharedString(mount_summary(ctx.mounts())));
    ui->set_mount_rows(string_model(rows_from_mounts(ctx.mounts())));
    auto diagnostics = ctx.diagnostics_json();
    ui->set_diagnostics(diagnostics ? slint::SharedString(*diagnostics)
                                    : slint::SharedString(UBAANext::Security::redact_sensitive_text(diagnostics.error().message)));
}

} // namespace

int main(int argc, char *argv[]) {
    const bool cli_probe = has_arg(argc, argv, "--version") || has_arg(argc, argv, "--diagnose") || has_arg(argc, argv, "--help");
    if (cli_probe) attach_parent_console();

    if (has_arg(argc, argv, "--version")) {
        write_stdout("UBAA Next Desktop " + std::string(UBAANEXT_VERSION_STRING) + "\n");
        return 0;
    }
    if (has_arg(argc, argv, "--help")) {
        write_stdout("UBAA Next Desktop includes an in-process Command Center for CLI commands.\n");
        write_stdout("Options: --version, --diagnose, --mock\n");
        return 0;
    }

    runtime::RuntimeOptions options;
    options.mock = has_arg(argc, argv, "--mock");
    auto runtime_result = runtime::RuntimeContext::create(options);
    if (!runtime_result) {
        write_stderr(UBAANext::Security::redact_sensitive_text(runtime_result.error().message) + "\n");
        return 2;
    }
    auto ctx = std::move(*runtime_result);

    if (has_arg(argc, argv, "--diagnose")) {
        auto diagnostics = ctx.diagnostics_json();
        if (!diagnostics) {
            write_stderr(diagnostics.error().message + "\n");
            return 2;
        }
        write_stdout(*diagnostics + "\n");
        return 0;
    }

    auto ui = MainWindow::create();
    ui->set_cloud_rows(string_model({"Select Refresh to load cloud roots"}));
    ui->set_campus_status(slint::SharedString("Select a campus feature."));
    ui->set_campus_rows(string_model({"Dedicated campus features use Runtime/Core; Command Center remains for long-tail commands."}));
    ui->set_command_output(slint::SharedString("Enter a CLI command and select Run."));
    refresh_runtime_state(ui, ctx);

    auto command_running = std::make_shared<std::atomic_bool>(false);
    auto campus_running = std::make_shared<std::atomic_bool>(false);
    ui->on_run_campus_feature([&ctx, ui, campus_running](slint::SharedString domain_text, slint::SharedString operation_text, slint::SharedString id_text, bool confirm) {
        bool expected = false;
        if (!campus_running->compare_exchange_strong(expected, true)) {
            ui->set_campus_status(slint::SharedString("A campus feature query is already running."));
            return;
        }

        auto domain = std::string(domain_text);
        auto operation = std::string(operation_text);
        auto id = std::string(id_text);
        ui->set_campus_status(slint::SharedString("Loading campus feature..."));
        slint::ComponentWeakHandle<MainWindow> weak_ui(ui);
        std::thread([&ctx, weak_ui, campus_running, domain = std::move(domain), operation = std::move(operation), id = std::move(id), confirm]() mutable {
            auto result = run_campus_feature(ctx, domain, operation, id, confirm);
            (void)slint::invoke_from_event_loop([weak_ui, campus_running, result = std::move(result)]() mutable {
                campus_running->store(false);
                if (auto ui = weak_ui.lock()) {
                    if (result) {
                        apply_feature_state(*ui, *result);
                    } else {
                        apply_feature_error(*ui, result.error());
                    }
                }
            });
        }).detach();
    });

    ui->on_run_command([ui, command_running](slint::SharedString command, bool confirm) {
        bool expected = false;
        if (!command_running->compare_exchange_strong(expected, true)) {
            ui->set_command_output(slint::SharedString("A CLI command is already running."));
            return;
        }

        auto arguments = split_command_line(std::string(command));
        if (arguments.empty()) {
            command_running->store(false);
            ui->set_command_output(slint::SharedString("No command entered."));
            return;
        }
        if (arguments.front() == "ubaa" || arguments.front() == "ubaa.com" || arguments.front() == "ubaa.exe" || arguments.front() == "ubaa-gui") {
            arguments.erase(arguments.begin());
        }
        if (!has_argument(arguments, "--json")) arguments.emplace_back("--json");
        if (confirm && !has_argument(arguments, "--confirm") && !has_argument(arguments, "--yes") && !has_argument(arguments, "-y")) {
            arguments.emplace_back("--confirm");
        }

        ui->set_command_output(slint::SharedString("Running CLI command..."));
        slint::ComponentWeakHandle<MainWindow> weak_ui(ui);
        std::thread([weak_ui, command_running, arguments = std::move(arguments)]() mutable {
            auto result = UBAANextCli::run_cli_command(arguments);
            auto output = format_command_result(result);
            (void)slint::invoke_from_event_loop([weak_ui, command_running, output = std::move(output)]() mutable {
                command_running->store(false);
                if (auto ui = weak_ui.lock()) (*ui)->set_command_output(slint::SharedString(output));
            });
        }).detach();
    });

    ui->on_login([&ctx, ui](slint::SharedString account, slint::SharedString password) {
        runtime::LoginRequest request;
        request.username = std::string(account);
        request.password = std::string(password);
        auto result = ctx.login(request);
        if (!result) {
            ui->set_session_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
            return;
        }
        refresh_runtime_state(ui, ctx);
    });
    ui->on_whoami([&ctx, ui]() {
        auto result = ctx.whoami();
        if (!result) {
            ui->set_session_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
            refresh_runtime_state(ui, ctx);
            return;
        }
        refresh_runtime_state(ui, ctx);
    });
    ui->on_logout([&ctx, ui](bool confirm) {
        if (!confirm) {
            ui->set_session_status(slint::SharedString("Check confirmation before clearing local session"));
            return;
        }
        auto result = ctx.logout();
        if (!result) ui->set_session_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
        refresh_runtime_state(ui, ctx);
    });
    ui->on_set_mode([&ctx, ui](slint::SharedString mode) {
        auto result = ctx.set_connection_mode(std::string(mode));
        if (!result) ui->set_session_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
        refresh_runtime_state(ui, ctx);
    });
    ui->on_set_theme([ui](slint::SharedString theme) {
        ui->set_theme(theme);
    });

    ui->on_refresh_cloud([&ctx, ui](slint::SharedString filter) {
        auto state = ctx.cloud_refresh(std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_open_cloud_path([&ctx, ui](slint::SharedString path, slint::SharedString filter) {
        auto state = std::string(path).empty() || std::string(path) == "/"
                         ? ctx.cloud_open_root()
                         : ctx.cloud_open_path(std::string(path), std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_upload_cloud([&ctx, ui](slint::SharedString local_path, bool overwrite, bool confirm, slint::SharedString filter) {
        auto state = ctx.cloud_upload_file(std::filesystem::path(std::string(local_path)), overwrite, confirm, std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            apply_cloud_state(ui, ctx.cloud_state(std::string(filter)));
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_download_cloud([&ctx, ui](slint::SharedString cloud_path, slint::SharedString local_path, bool overwrite, bool confirm, slint::SharedString filter) {
        if (!confirm) {
            apply_cloud_error(ui, {UBAANext::ErrorCode::InvalidArgument, "Check confirmation before writing local download file"});
            return;
        }
        auto state = ctx.cloud_download_file(std::string(cloud_path), std::filesystem::path(std::string(local_path)), overwrite, std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            apply_cloud_state(ui, ctx.cloud_state(std::string(filter)));
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_delete_cloud([&ctx, ui](slint::SharedString cloud_path, bool confirm, slint::SharedString filter) {
        auto state = ctx.cloud_delete(std::string(cloud_path), confirm, std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_rename_cloud([&ctx, ui](slint::SharedString cloud_path, slint::SharedString name, bool confirm, slint::SharedString filter) {
        auto state = ctx.cloud_rename(std::string(cloud_path), std::string(name), confirm, std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            return;
        }
        apply_cloud_state(ui, *state);
    });
    ui->on_create_cloud_folder([&ctx, ui](slint::SharedString name, bool confirm, slint::SharedString filter) {
        auto state = ctx.cloud_create_dir(std::string(name), confirm, std::string(filter));
        if (!state) {
            apply_cloud_error(ui, state.error());
            return;
        }
        apply_cloud_state(ui, *state);
    });

    auto start_mount = [&ctx, ui](runtime::CloudMountFrontend frontend, bool writable, slint::SharedString mount_point) {
        runtime::CloudMountRequest request;
        request.frontend = frontend;
        request.account_key = "default";
        request.writable = writable;
        auto mount_text = std::string(mount_point);
        request.mount_point = mount_text.empty()
                                  ? (frontend == runtime::CloudMountFrontend::WinFsp ? std::filesystem::path("B:\\")
                                                                                     : ctx.app_data_dir() / "mount")
                                  : std::filesystem::path(mount_text);
        request.cache_dir = ctx.cache_dir();
        auto result = ctx.mounts().start(request);
        if (!result) ui->set_mount_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
        refresh_runtime_state(ui, ctx);
    };
    ui->on_start_winfsp([&](bool writable, slint::SharedString mount_point) {
        start_mount(runtime::CloudMountFrontend::WinFsp, writable, mount_point);
    });
    ui->on_start_cloud_files([&](bool writable, slint::SharedString mount_point) {
        start_mount(runtime::CloudMountFrontend::CloudFiles, writable, mount_point);
    });
    ui->on_start_fuse([&](bool writable, slint::SharedString mount_point) {
        start_mount(runtime::CloudMountFrontend::Fuse, writable, mount_point);
    });
    ui->on_stop_mounts([&ctx, ui](bool confirm) {
        if (!confirm) {
            ui->set_mount_status(slint::SharedString("Check confirmation before stopping mount frontends"));
            return;
        }
        auto statuses = ctx.mounts().statuses();
        for (const auto &status : statuses) {
            if (status.running) (void)ctx.mounts().stop(status.frontend);
        }
        refresh_runtime_state(ui, ctx);
    });
    ui->on_clear_cache([&ctx, ui](bool confirm) {
        if (!confirm) {
            ui->set_cache_status(slint::SharedString("Check confirmation before clearing local cache"));
            return;
        }
        auto result = ctx.clear_cache(confirm);
        if (!result) ui->set_cache_status(slint::SharedString(UBAANext::Security::redact_sensitive_text(result.error().message)));
        refresh_runtime_state(ui, ctx);
    });
    ui->on_export_diagnostics([&ctx, ui]() { refresh_runtime_state(ui, ctx); });

    return ui->run();
}
