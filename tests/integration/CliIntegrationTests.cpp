/**
 * @file CliIntegrationTests.cpp
 * @brief CLI 集成测试
 *
 * 测试 CLI 命令的 JSON 输出格式和 exit code。
 * 使用 --mock 模式避免真实网络调用。
 */

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ubaa 可执行文件路径（通过 CMake 定义）
#ifndef UBAA_CLI_PATH
#define UBAA_CLI_PATH "ubaa"
#endif

struct CliResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
};

[[nodiscard]] CliResult run_cli(const std::vector<std::string> &args) {
    // 构建命令行 - 用引号包裹路径以防空格
    static const auto app_data_dir = (std::filesystem::temp_directory_path() / "ubaanext-cli-tests").string();
    std::filesystem::create_directories(app_data_dir);

    std::string cmd = "set \"UBAANEXT_APP_DATA_DIR=";
    cmd += app_data_dir;
    cmd += "\" && \"";
    cmd += UBAA_CLI_PATH;
    cmd += "\"";
    for (const auto &arg : args) {
        cmd += " " + arg;
    }
    cmd += " 2>&1";  // 合并 stderr

    CliResult result;
    std::array<char, 4096> buffer;

    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
    if (!pipe) {
        result.exit_code = -1;
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result.stdout_output += buffer.data();
    }

    result.exit_code = _pclose(pipe.release());
    // Windows _popen 返回的是原始退出码
    return result;
}

[[nodiscard]] nlohmann::json parse_json_output(const std::string &output) {
    // 找到第一个 '{' 字符，跳过可能的前导内容
    auto start = output.find('{');
    if (start == std::string::npos) {
        throw std::runtime_error("No JSON object found in output: " + output);
    }
    return nlohmann::json::parse(output.substr(start));
}

void require_success_envelope(const nlohmann::json &json) {
    REQUIRE(json["ok"] == true);
    REQUIRE(json.contains("data"));
    REQUIRE(json["data"].is_object());
    REQUIRE(json.contains("error"));
    REQUIRE(json["error"].is_null());
}

void require_error_envelope(const nlohmann::json &json) {
    REQUIRE(json["ok"] == false);
    REQUIRE(json.contains("data"));
    REQUIRE(json["data"].is_null());
    REQUIRE(json.contains("error"));
    REQUIRE(json["error"].is_object());
    REQUIRE(json["error"].contains("code"));
    REQUIRE(json["error"].contains("message"));
}

void require_feature_record(const nlohmann::json &record) {
    REQUIRE(record.is_object());
    REQUIRE(record.contains("id"));
    REQUIRE(record.contains("title"));
    REQUIRE(record.contains("status"));
    REQUIRE(record.contains("fields"));
    REQUIRE(record["fields"].is_object());
}

void require_records_contract(const nlohmann::json &json, const std::string &key) {
    require_success_envelope(json);
    REQUIRE(json["data"].contains(key));
    REQUIRE(json["data"][key].is_array());
    REQUIRE_FALSE(json["data"][key].empty());
    require_feature_record(json["data"][key][0]);
}

void require_mutation_contract(const nlohmann::json &json) {
    require_success_envelope(json);
    REQUIRE(json["data"].contains("accepted"));
    REQUIRE(json["data"].contains("message"));
    REQUIRE(json["data"].contains("result"));
    REQUIRE(json["data"]["accepted"].is_boolean());
    REQUIRE(json["data"]["message"].is_string());
    require_feature_record(json["data"]["result"]);
}

} // namespace

TEST_CASE("CLI version 命令", "[cli][integration]") {
    auto result = run_cli({"version", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("version"));
    REQUIRE_FALSE(json["data"]["version"].get<std::string>().empty());
}

TEST_CASE("CLI help 命令", "[cli][integration]") {
    const std::vector<std::string> expected_commands = {
        "version",
        "help",
        "login",
        "mode",
        "mode direct",
        "mode vpn",
        "whoami",
        "logout",
        "course today",
        "course date",
        "course week",
        "exam list",
        "classroom query",
        "term list",
        "week list",
        "grade list",
        "grade all",
        "user info",
        "app version",
        "app announcement",
        "spoc assignments",
        "spoc assignment show",
        "judge assignments",
        "judge assignment show",
        "judge assignment details",
        "judge assignment details-batch",
        "signin today",
        "signin do",
        "ygdk overview",
        "ygdk records",
        "ygdk submit",
        "evaluation list",
        "evaluation submit",
        "bykc profile",
        "bykc courses",
        "bykc chosen",
        "bykc stats",
        "bykc course show",
        "bykc select",
        "bykc unselect",
        "bykc sign",
        "cgyy sites",
        "cgyy purpose-types",
        "cgyy day-info",
        "cgyy orders",
        "cgyy order show",
        "cgyy order lock-code",
        "cgyy reserve",
        "cgyy order cancel",
        "libbook libraries",
        "libbook areas",
        "libbook seats",
        "libbook reservations",
        "libbook area show",
        "libbook book",
        "libbook cancel",
        "todo list",
        "file upload",
        "config show",
        "config set",
        "cache clear",
    };

    auto json_result = run_cli({"help", "--json"});
    REQUIRE(json_result.exit_code == 0);

    auto json = parse_json_output(json_result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("commands"));
    REQUIRE(json["data"].contains("version"));
    REQUIRE(json["data"]["commands"].is_array());
    REQUIRE_FALSE(json["data"]["commands"].empty());

    const auto &commands = json["data"]["commands"];
    auto find_command = [&](const std::string &name) -> const nlohmann::json * {
        auto it = std::find_if(commands.begin(), commands.end(), [&](const auto &command) {
            return command.contains("name") && command["name"] == name;
        });
        return it == commands.end() ? nullptr : &(*it);
    };
    auto has_command = [&](const std::string &name) {
        return find_command(name) != nullptr;
    };
    auto find_option = [&](const std::string &command_name, const std::string &option_name) -> const nlohmann::json * {
        const auto *command = find_command(command_name);
        if (command == nullptr || !command->contains("options")) return nullptr;
        const auto &options = (*command)["options"];
        auto it = std::find_if(options.begin(), options.end(), [&](const auto &option) {
            return option.contains("name") && option["name"] == option_name;
        });
        return it == options.end() ? nullptr : &(*it);
    };
    for (const auto &expected_command : expected_commands) {
        INFO("缺失 JSON help 命令: " << expected_command);
        CHECK(has_command(expected_command));
    }

    auto spoc_id_option = find_option("spoc assignment show", "--id");
    REQUIRE(spoc_id_option != nullptr);
    CHECK((*spoc_id_option)["required"] == true);
    CHECK((*spoc_id_option)["placeholder"] == "assignment-id");
    CHECK((*spoc_id_option)["sourceCommand"] == "spoc assignments");
    CHECK((*spoc_id_option)["sourceField"] == "id");

    auto cgyy_time_option = find_option("cgyy reserve", "--id");
    REQUIRE(cgyy_time_option != nullptr);
    CHECK((*cgyy_time_option)["placeholder"] == "time-id");
    CHECK((*cgyy_time_option)["sourceCommand"] == "cgyy day-info");
    CHECK((*cgyy_time_option)["sourceField"] == "fields.timeId");

    auto libbook_booking_option = find_option("libbook cancel", "--booking-id");
    REQUIRE(libbook_booking_option != nullptr);
    CHECK((*libbook_booking_option)["placeholder"] == "booking-id");
    CHECK((*libbook_booking_option)["sourceCommand"] == "libbook reservations");
    CHECK((*libbook_booking_option)["sourceField"] == "id");

    auto text_result = run_cli({"help"});
    REQUIRE(text_result.exit_code == 0);
    for (const auto &expected_command : expected_commands) {
        INFO("缺失普通 help 命令: " << expected_command);
        CHECK(text_result.stdout_output.find(expected_command) != std::string::npos);
    }
    const std::vector<std::string> expected_help_tokens = {
        "<assignment-id>",
        "<signin-id>",
        "<course-id>",
        "<site-id>",
        "<order-id>",
        "<library-id>",
        "<area-id>",
        "<seat-id>",
        "<booking-id>",
        "spoc assignments 输出记录的 id 字段",
        "cgyy day-info 的 id，time-id 来自 fields.timeId",
        "libbook reservations 输出记录的 id 字段",
        "help --json 输出机器可读命令目录",
    };
    for (const auto &expected_token : expected_help_tokens) {
        INFO("普通 help 缺失清晰参数提示: " << expected_token);
        CHECK(text_result.stdout_output.find(expected_token) != std::string::npos);
    }
}

#if UBAANEXT_ENABLE_MOCKS
TEST_CASE("CLI login mock 命令", "[cli][integration]") {
    auto result = run_cli({"login", "--mock", "20260000", "test", "--json"});
    REQUIRE(result.exit_code == 0);

    // 登录命令输出两个 JSON 对象（多行），检查包含关键字段
    REQUIRE(result.stdout_output.find("\"ok\": true") != std::string::npos);
    REQUIRE(result.stdout_output.find("\"studentId\"") != std::string::npos);
    REQUIRE(result.stdout_output.find("20260000") != std::string::npos);
    REQUIRE(result.stdout_output.find("登录成功") != std::string::npos);
}

TEST_CASE("CLI login mock 兼容旧参数", "[cli][integration]") {
    auto result = run_cli({"login", "--mock", "--username", "20260000", "--password", "test", "--json"});
    REQUIRE(result.exit_code == 0);

    REQUIRE(result.stdout_output.find("\"ok\": true") != std::string::npos);
    REQUIRE(result.stdout_output.find("\"studentId\"") != std::string::npos);
    REQUIRE(result.stdout_output.find("20260000") != std::string::npos);
}

TEST_CASE("CLI whoami 命令", "[cli][integration]") {
    // 先登录
    auto login_result = run_cli({"login", "--mock", "--username", "20260000", "--password", "test", "--json"});
    REQUIRE(login_result.exit_code == 0);

    auto result = run_cli({"whoami", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("studentId"));
    REQUIRE(json["data"]["studentId"] == "20260000");
}

TEST_CASE("CLI logout 命令", "[cli][integration]") {
    // 先登录
    auto login_result = run_cli({"login", "--mock", "--username", "20260000", "--password", "test", "--json"});
    REQUIRE(login_result.exit_code == 0);

    auto result = run_cli({"logout", "--confirm", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("message"));

    auto whoami = run_cli({"whoami", "--json"});
    REQUIRE(whoami.exit_code != 0);
    auto whoami_json = parse_json_output(whoami.stdout_output);
    require_error_envelope(whoami_json);
}

TEST_CASE("CLI course today mock 命令", "[cli][integration]") {
    auto result = run_cli({"course", "today", "--mock", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("courses"));
    REQUIRE(json["data"]["courses"].is_array());
    REQUIRE_FALSE(json["data"]["courses"].empty());

    // 检查课程字段
    auto &course = json["data"]["courses"][0];
    REQUIRE(course.contains("name"));
    REQUIRE(course.contains("teacher"));
    REQUIRE(course.contains("classroom"));
    REQUIRE(course.contains("sectionStart"));
    REQUIRE(course.contains("sectionEnd"));
}

TEST_CASE("CLI course week mock 命令", "[cli][integration]") {
    auto result = run_cli({"course", "week", "--mock", "--week", "8", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("courses"));
    REQUIRE(json["data"]["courses"].is_array());
}

TEST_CASE("CLI exam list mock 命令", "[cli][integration]") {
    auto result = run_cli({"exam", "list", "--mock", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("exams"));
    REQUIRE(json["data"]["exams"].is_array());
    REQUIRE_FALSE(json["data"]["exams"].empty());

    // 检查考试字段
    auto &exam = json["data"]["exams"][0];
    REQUIRE(exam.contains("courseName"));
    REQUIRE(exam.contains("location"));
    REQUIRE(exam.contains("timeText"));
}

TEST_CASE("CLI classroom query mock 命令", "[cli][integration]") {
    auto result = run_cli({"classroom", "query", "--mock", "--campus", "1", "--date", "2026-05-13", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("buildings"));
}

TEST_CASE("CLI term list mock 命令", "[cli][integration]") {
    auto result = run_cli({"term", "list", "--mock", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("terms"));
    REQUIRE(json["data"]["terms"].is_array());
    REQUIRE_FALSE(json["data"]["terms"].empty());

    // 检查学期字段
    auto &term = json["data"]["terms"][0];
    REQUIRE(term.contains("code"));
    REQUIRE(term.contains("name"));
    REQUIRE(term.contains("selected"));
}

TEST_CASE("CLI week list mock 命令", "[cli][integration]") {
    auto result = run_cli({"week", "list", "--mock", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("weeks"));
    REQUIRE(json["data"]["weeks"].is_array());
    REQUIRE_FALSE(json["data"]["weeks"].empty());

    // 检查周次字段
    auto &week = json["data"]["weeks"][0];
    REQUIRE(week.contains("serialNumber"));
    REQUIRE(week.contains("name"));
    REQUIRE(week.contains("startDate"));
    REQUIRE(week.contains("endDate"));
}

TEST_CASE("CLI 人类可读输出使用表格列名", "[cli][integration]") {
    auto assignments = run_cli({"spoc", "assignments", "--mock"});
    REQUIRE(assignments.exit_code == 0);
    CHECK(assignments.stdout_output.find("Assignments") != std::string::npos);
    CHECK(assignments.stdout_output.find("Index") != std::string::npos);
    CHECK(assignments.stdout_output.find("Title") != std::string::npos);
    CHECK(assignments.stdout_output.find("Status") != std::string::npos);
    CHECK(assignments.stdout_output.find("Id") != std::string::npos);

    auto courses = run_cli({"course", "today", "--mock"});
    REQUIRE(courses.exit_code == 0);
    CHECK(courses.stdout_output.find("Courses - Today") != std::string::npos);
    CHECK(courses.stdout_output.find("Name") != std::string::npos);
    CHECK(courses.stdout_output.find("Teacher") != std::string::npos);
    CHECK(courses.stdout_output.find("Classroom") != std::string::npos);
    CHECK(courses.stdout_output.find("Id") != std::string::npos);

    auto detail = run_cli({"spoc", "assignment", "show", "--mock", "--id", "spoc-1"});
    REQUIRE(detail.exit_code == 0);
    CHECK(detail.stdout_output.find("Assignment") != std::string::npos);
    CHECK(detail.stdout_output.find("Field") != std::string::npos);
    CHECK(detail.stdout_output.find("Value") != std::string::npos);
    CHECK(detail.stdout_output.find("Title") != std::string::npos);
    CHECK(detail.stdout_output.find("Status") != std::string::npos);
    CHECK(detail.stdout_output.find("Id") != std::string::npos);
}

TEST_CASE("CLI v0.4 golden help contract", "[cli][integration][golden]") {
    auto result = run_cli({"help", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    require_success_envelope(json);
    REQUIRE(json["data"].contains("commands"));
    REQUIRE(json["data"].contains("version"));

    const auto &commands = json["data"]["commands"];
    REQUIRE(commands.is_array());

    std::vector<std::string> names;
    for (const auto &command : commands) {
        REQUIRE(command.contains("name"));
        REQUIRE(command.contains("description"));
        names.push_back(command["name"].get<std::string>());
    }

    auto contains_name = [&](const std::string &name) {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    CHECK(contains_name("config show"));
    CHECK(contains_name("config set"));
    CHECK(contains_name("cache clear"));
    CHECK(contains_name("course today"));
    CHECK(contains_name("todo list"));
    CHECK(contains_name("file upload"));

    auto duplicate = std::adjacent_find(names.begin(), names.end());
    std::sort(names.begin(), names.end());
    duplicate = std::adjacent_find(names.begin(), names.end());
    CHECK(duplicate == names.end());
}

TEST_CASE("CLI v0.4 golden exit code contract", "[cli][integration][golden]") {
    const std::vector<std::pair<std::vector<std::string>, int>> commands = {
        {{"version", "--json"}, 0},
        {{"unknown", "--json"}, 2},
        {{"config", "set", "--key", "cache", "--value", "false", "--json"}, 2},
        {{"cache", "clear", "--json"}, 2},
    };

    for (const auto &[command, expected_exit] : commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == expected_exit);
        CHECK(result.exit_code >= 0);
        CHECK(result.exit_code <= 6);
        auto json = parse_json_output(result.stdout_output);
        if (expected_exit == 0) {
            require_success_envelope(json);
        } else {
            require_error_envelope(json);
        }
    }
}

TEST_CASE("CLI v0.4 config/cache confirm gates", "[cli][integration][golden]") {
    auto set_without_confirm = run_cli({"config", "set", "--key", "cache", "--value", "false", "--json"});
    REQUIRE(set_without_confirm.exit_code == 2);
    auto set_without_confirm_json = parse_json_output(set_without_confirm.stdout_output);
    require_error_envelope(set_without_confirm_json);
    CHECK(set_without_confirm_json["error"]["message"].get<std::string>().find("--confirm") != std::string::npos);

    auto set_with_confirm = run_cli({"config", "set", "--key", "cache", "--value", "false", "--confirm", "--json"});
    REQUIRE(set_with_confirm.exit_code == 0);
    auto set_with_confirm_json = parse_json_output(set_with_confirm.stdout_output);
    require_success_envelope(set_with_confirm_json);
    REQUIRE(set_with_confirm_json["data"].contains("message"));
    CHECK(set_with_confirm_json["data"]["message"].get<std::string>().find("cache = false") != std::string::npos);

    auto show_after_set = run_cli({"config", "show", "--json"});
    REQUIRE(show_after_set.exit_code == 0);
    auto show_after_set_json = parse_json_output(show_after_set.stdout_output);
    require_success_envelope(show_after_set_json);
    REQUIRE(show_after_set_json["data"].contains("cacheEnabled"));
    CHECK(show_after_set_json["data"]["cacheEnabled"] == false);

    auto clear_without_confirm = run_cli({"cache", "clear", "--json"});
    REQUIRE(clear_without_confirm.exit_code == 2);
    auto clear_without_confirm_json = parse_json_output(clear_without_confirm.stdout_output);
    require_error_envelope(clear_without_confirm_json);
    CHECK(clear_without_confirm_json["error"]["message"].get<std::string>().find("--confirm") != std::string::npos);

    auto clear_with_confirm = run_cli({"cache", "clear", "--confirm", "--json"});
    REQUIRE(clear_with_confirm.exit_code == 0);
    require_success_envelope(parse_json_output(clear_with_confirm.stdout_output));
}

TEST_CASE("CLI config show 命令", "[cli][integration]") {
    auto result = run_cli({"config", "show", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("mode"));
    REQUIRE(json["data"].contains("version"));
}

#if !defined(_WIN32)
TEST_CASE("CLI real login fails closed without secure store", "[cli][integration][security]") {
    auto result = run_cli({"login", "--username", "20260000", "--password", "test", "--json"});
    REQUIRE(result.exit_code != 0);

    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"].get<std::string>().find("UnsupportedSecureStore") != std::string::npos);
}
#endif

TEST_CASE("CLI config proxy 输出会脱敏凭据", "[cli][integration]") {
    auto set_result = run_cli({"config", "set", "--key", "proxy", "--value", "http://user:secret@example.com:8080", "--confirm", "--json"});
    REQUIRE(set_result.exit_code == 0);
    REQUIRE(set_result.stdout_output.find("user:secret") == std::string::npos);
    REQUIRE(set_result.stdout_output.find("[REDACTED]") != std::string::npos);

    auto show_result = run_cli({"config", "show", "--json"});
    REQUIRE(show_result.exit_code == 0);
    REQUIRE(show_result.stdout_output.find("user:secret") == std::string::npos);
    auto json = parse_json_output(show_result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"]["proxy"].get<std::string>() == "http://[REDACTED]@example.com:8080");

    auto clear_result = run_cli({"config", "set", "--key", "proxy", "--value", "none", "--confirm", "--json"});
    REQUIRE(clear_result.exit_code == 0);
}

TEST_CASE("CLI cache clear 命令", "[cli][integration]") {
    auto result = run_cli({"cache", "clear", "--confirm", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("message"));
}

TEST_CASE("CLI 新增只读命令 mock smoke", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"user", "info", "--mock", "--json"},
        {"app", "version", "--mock", "--json"},
        {"app", "announcement", "--mock", "--json"},
        {"grade", "list", "--mock", "--term", "2025-2026-2", "--json"},
        {"grade", "list", "--all", "--mock", "--json"},
        {"grade", "all", "--mock", "--json"},
        {"classroom", "query", "--mock", "--campus", "1", "--date", "2026-05-15", "--sections", "1,2", "--json"},
        {"spoc", "assignments", "--mock", "--pending-only", "--include-expired", "--json"},
        {"spoc", "assignment", "show", "--mock", "--id", "spoc-1", "--json"},
        {"judge", "assignments", "--mock", "--course-id", "course-1", "--include-history", "--include-expired", "--json"},
        {"judge", "assignment", "show", "--mock", "--assignment-id", "judge-1", "--json"},
        {"judge", "assignment", "details", "--mock", "--assignment-id", "judge-1", "--json"},
        {"judge", "assignment", "details-batch", "--mock", "--input", "judge-1", "--json"},
        {"signin", "today", "--mock", "--json"},
        {"ygdk", "overview", "--mock", "--json"},
        {"ygdk", "records", "--mock", "--json"},
        {"ygdk", "records", "--mock", "--page", "1", "--limit", "20", "--json"},
        {"evaluation", "list", "--mock", "--json"},
        {"todo", "list", "--mock", "--json"},
        {"todo", "list", "--mock", "--all", "--json"},
        {"bykc", "profile", "--mock", "--json"},
        {"bykc", "courses", "--mock", "--status", "available", "--category", "通识", "--keyword", "课程", "--json"},
        {"bykc", "chosen", "--mock", "--json"},
        {"bykc", "course", "show", "--mock", "--course-id", "bykc-1", "--json"},
        {"bykc", "stats", "--mock", "--json"},
        {"cgyy", "sites", "--mock", "--json"},
        {"cgyy", "purpose-types", "--mock", "--json"},
        {"cgyy", "day-info", "--mock", "--site-id", "1", "--date", "2026-05-15", "--json"},
        {"cgyy", "order", "show", "--mock", "--order-id", "cgyy-1", "--json"},
        {"cgyy", "order", "lock-code", "--mock", "--order-id", "cgyy-1", "--json"},
        {"libbook", "libraries", "--mock", "--json"},
        {"libbook", "areas", "--mock", "--library-id", "libbook-1", "--json"},
        {"libbook", "area", "show", "--mock", "--area-id", "libbook-1", "--json"},
        {"libbook", "seats", "--mock", "--area-id", "libbook-1", "--json"},
        {"libbook", "reservations", "--mock", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == 0);
        auto json = parse_json_output(result.stdout_output);
        REQUIRE(json["ok"] == true);
        REQUIRE(json["data"].is_object());
    }
}

TEST_CASE("CLI 有副作用命令需要 confirm", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"logout", "--json"},
        {"config", "set", "--key", "mode", "--value", "direct", "--json"},
        {"cache", "clear", "--json"},
        {"signin", "do", "--mock", "--json"},
        {"ygdk", "submit", "--mock", "--json"},
        {"evaluation", "submit", "--mock", "--json"},
        {"bykc", "select", "--mock", "--course-id", "bykc-1", "--json"},
        {"bykc", "unselect", "--mock", "--course-id", "bykc-1", "--json"},
        {"bykc", "sign", "--mock", "--course-id", "bykc-1", "--sign-type", "1", "--json"},
        {"cgyy", "reserve", "--mock", "--json"},
        {"cgyy", "order", "cancel", "--mock", "--order-id", "cgyy-1", "--json"},
        {"libbook", "book", "--mock", "--seat-id", "libbook-1", "--json"},
        {"libbook", "cancel", "--mock", "--booking-id", "libbook-1", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        REQUIRE(json["ok"] == false);
        REQUIRE(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 直接服务写操作保留分隔符参数", "[cli][integration]") {
    auto signin = run_cli({"signin", "do", "--mock", "--id", "signin:1", "--confirm", "--json"});
    REQUIRE(signin.exit_code == 0);
    auto signin_json = parse_json_output(signin.stdout_output);
    REQUIRE(signin_json["ok"] == true);

    auto ygdk = run_cli({"ygdk", "submit", "--mock", "--id", "ygdk:1", "--start-time", "08:00", "--end-time", "09:30", "--place", "操场:东区", "--photo", "C:\\tmp\\a:b.jpg", "--share", "--confirm", "--json"});
    REQUIRE(ygdk.exit_code == 0);
    auto ygdk_json = parse_json_output(ygdk.stdout_output);
    REQUIRE(ygdk_json["ok"] == true);

    auto evaluation = run_cli({"evaluation", "submit", "--mock", "--id", "evaluation:1", "--confirm", "--json"});
    REQUIRE(evaluation.exit_code == 0);
    auto evaluation_json = parse_json_output(evaluation.stdout_output);
    REQUIRE(evaluation_json["ok"] == true);

    auto bykc = run_cli({"bykc", "sign", "--mock", "--course-id", "bykc:1", "--sign-type", "1", "--confirm", "--json"});
    REQUIRE(bykc.exit_code == 0);
    auto bykc_json = parse_json_output(bykc.stdout_output);
    REQUIRE(bykc_json["ok"] == true);

    auto cgyy = run_cli({"cgyy", "reserve", "--mock", "--id", "time:1", "--site-id", "site:1", "--space-id", "space:1", "--date", "2026-05-15", "--purpose-type", "purpose:1", "--theme", "组会:安全", "--phone", "13800000000", "--joiners", "张三\\n李四", "--captcha", "captcha:1", "--token", "token:1", "--confirm", "--json"});
    REQUIRE(cgyy.exit_code == 0);
    auto cgyy_json = parse_json_output(cgyy.stdout_output);
    REQUIRE(cgyy_json["ok"] == true);

    auto libbook = run_cli({"libbook", "book", "--mock", "--seat-id", "seat:1", "--date", "2026-05-15", "--segment", "08:00-10:00\\n10:00-12:00", "--confirm", "--json"});
    REQUIRE(libbook.exit_code == 0);
    auto libbook_json = parse_json_output(libbook.stdout_output);
    REQUIRE(libbook_json["ok"] == true);
}

TEST_CASE("CLI 图书馆预约 confirm 后仍校验必要参数", "[cli][integration]") {
    auto result = run_cli({"libbook", "book", "--mock", "--seat-id", "libbook-1", "--confirm", "--json"});

    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 有副作用命令 confirm 后仍校验业务 ID", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"bykc", "select", "--mock", "--confirm", "--json"},
        {"bykc", "unselect", "--mock", "--confirm", "--json"},
        {"cgyy", "order", "cancel", "--mock", "--confirm", "--json"},
        {"libbook", "cancel", "--mock", "--confirm", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI CGYY mock 模式仍校验必要参数", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"cgyy", "day-info", "--mock", "--json"},
        {"cgyy", "order", "show", "--mock", "--json"},
        {"cgyy", "reserve", "--mock", "--confirm", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 只读 mock 模式仍校验必要业务 ID", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"spoc", "assignment", "show", "--mock", "--json"},
        {"judge", "assignment", "show", "--mock", "--json"},
        {"judge", "assignment", "details", "--mock", "--json"},
        {"bykc", "course", "show", "--mock", "--json"},
        {"libbook", "areas", "--mock", "--json"},
        {"libbook", "seats", "--mock", "--json"},
        {"libbook", "area", "show", "--mock", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 有副作用命令 confirm 后 mock 可执行", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"signin", "do", "--mock", "--id", "signin-1", "--confirm", "--json"},
        {"ygdk", "submit", "--mock", "--confirm", "--json"},
        {"evaluation", "submit", "--mock", "--confirm", "--json"},
        {"bykc", "select", "--mock", "--course-id", "bykc-1", "--confirm", "--json"},
        {"bykc", "unselect", "--mock", "--course-id", "bykc-1", "--confirm", "--json"},
        {"bykc", "sign", "--mock", "--course-id", "bykc-1", "--sign-type", "1", "--confirm", "--json"},
        {"cgyy", "reserve", "--mock", "--id", "1", "--site-id", "1", "--space-id", "1", "--date", "2026-05-15", "--purpose-type", "1", "--theme", "组会", "--phone", "13800000000", "--joiners", "张三", "--captcha", "captcha", "--token", "token", "--confirm", "--json"},
        {"cgyy", "order", "cancel", "--mock", "--order-id", "cgyy-1", "--confirm", "--json"},
        {"libbook", "book", "--mock", "--seat-id", "libbook-1", "--date", "2026-05-15", "--segment", "08:00-10:00", "--confirm", "--json"},
        {"libbook", "cancel", "--mock", "--booking-id", "libbook-1", "--confirm", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == 0);
        auto json = parse_json_output(result.stdout_output);
        REQUIRE(json["ok"] == true);
        REQUIRE(json["data"].contains("message"));
    }
}

TEST_CASE("CLI Judge details-batch mock 保留 partial failure 记录", "[cli][integration]") {
    auto result = run_cli({"judge", "assignment", "details-batch", "--mock", "--input", "judge-1,judge-error", "--json"});
    INFO(result.stdout_output);
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    require_success_envelope(json);
    REQUIRE(json["data"].contains("details"));
    REQUIRE(json["data"]["details"].is_array());
    REQUIRE(json["data"]["details"].size() == 2);

    const auto &success = json["data"]["details"][0];
    require_feature_record(success);
    CHECK(success["id"] == "judge-1");
    CHECK(success["status"] == "unsubmitted");

    const auto &failed = json["data"]["details"][1];
    require_feature_record(failed);
    CHECK(failed["id"] == "judge-error");
    CHECK(failed["status"] == "error");
    REQUIRE(failed["fields"].contains("submissionStatusText"));
    CHECK(failed["fields"]["submissionStatusText"] == "NetworkError");
    REQUIRE(failed["fields"].contains("content"));
    const auto content = failed["fields"]["content"].get<std::string>();
    CHECK(content.find("captcha-secret") == std::string::npos);
    CHECK(content.find("bearer-secret") == std::string::npos);
    CHECK(content.find("C:/secret/judge.html") == std::string::npos);
    CHECK(content.find("[REDACTED]") != std::string::npos);
}

TEST_CASE("CLI JSON contract mock smoke", "[cli][integration]") {
    const std::vector<std::pair<std::vector<std::string>, std::string>> list_commands = {
        {{"signin", "today", "--mock", "--json"}, "signin"},
        {{"ygdk", "overview", "--mock", "--json"}, "overview"},
        {{"ygdk", "records", "--mock", "--json"}, "records"},
        {{"evaluation", "list", "--mock", "--json"}, "evaluations"},
        {{"todo", "list", "--mock", "--json"}, "todos"},
        {{"bykc", "courses", "--mock", "--json"}, "courses"},
        {{"cgyy", "sites", "--mock", "--json"}, "sites"},
        {{"libbook", "libraries", "--mock", "--json"}, "libraries"},
    };

    for (const auto &[command, key] : list_commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == 0);
        require_records_contract(parse_json_output(result.stdout_output), key);
    }

    auto detail = run_cli({"bykc", "course", "show", "--mock", "--course-id", "bykc-1", "--json"});
    REQUIRE(detail.exit_code == 0);
    auto detail_json = parse_json_output(detail.stdout_output);
    require_success_envelope(detail_json);
    REQUIRE(detail_json["data"].contains("course"));
    require_feature_record(detail_json["data"]["course"]);

    auto mutation = run_cli({"bykc", "select", "--mock", "--course-id", "bykc-1", "--confirm", "--json"});
    REQUIRE(mutation.exit_code == 0);
    require_mutation_contract(parse_json_output(mutation.stdout_output));

    auto error = run_cli({"bykc", "select", "--mock", "--course-id", "bykc-1", "--json"});
    REQUIRE(error.exit_code == 2);
    auto error_json = parse_json_output(error.stdout_output);
    require_error_envelope(error_json);
    REQUIRE(error_json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 保留占位接口稳定返回 NotImplemented", "[cli][integration]") {
    auto result = run_cli({"file", "upload", "--path", "C:\\tmp\\attachment.jpg", "--confirm", "--json"});
    REQUIRE(result.exit_code != 0);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "NotImplemented");
}

TEST_CASE("CLI 占位上传接口仍需要显式确认", "[cli][integration]") {
    auto result = run_cli({"file", "upload", "--path", "C:\\tmp\\attachment.jpg", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 默认写能力开启后打卡上传继续执行到下一层校验", "[cli][integration]") {
    auto result = run_cli({"ygdk", "submit", "--id", "ygdk-1", "--start-time", "08:00", "--end-time", "09:00", "--place", "操场", "--photo", "C:\\tmp\\missing-ygdk-photo.jpg", "--confirm", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
    CHECK(json["error"]["message"] == "无法读取上传文件");
}

TEST_CASE("CLI -y 可确认真实写操作", "[cli][integration]") {
    auto result = run_cli({"ygdk", "submit", "--id", "ygdk-1", "--start-time", "08:00", "--end-time", "09:00", "--place", "操场", "--photo", "C:\\tmp\\missing-ygdk-photo.jpg", "-y", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
    CHECK(json["error"]["message"] == "无法读取上传文件");
}

TEST_CASE("CLI JSON 模式写操作缺少确认时 fail closed", "[cli][integration]") {
    auto result = run_cli({"ygdk", "submit", "--id", "ygdk-1", "--start-time", "08:00", "--end-time", "09:00", "--place", "操场", "--photo", "C:\\tmp\\missing-ygdk-photo.jpg", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
    const auto message = json["error"]["message"].get<std::string>();
    CHECK(message.find("--confirm") != std::string::npos);
    CHECK(message.find("--yes") != std::string::npos);
    CHECK(message.find("-y") != std::string::npos);
    CHECK(message.find("交互输入 y") != std::string::npos);
}

#endif

TEST_CASE("CLI 签到真实写操作要求课程 ID", "[cli][integration]") {
    auto result = run_cli({"signin", "do", "--confirm", "--json"});
    REQUIRE(result.exit_code == 2);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == false);
    REQUIRE(json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 博雅签到要求显式课程和签到类型", "[cli][integration]") {
    auto result = run_cli({"bykc", "sign", "--course-id", "1", "--confirm", "--json"});
    REQUIRE(result.exit_code == 2);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == false);
    REQUIRE(json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 公告真实模式返回稳定空列表", "[cli][integration]") {
    auto result = run_cli({"app", "announcement", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("announcements"));
    REQUIRE(json["data"]["announcements"].is_array());
}

TEST_CASE("CLI 图书馆座位真实模式要求查询 ID", "[cli][integration]") {
    auto areas_result = run_cli({"libbook", "areas", "--json"});
    REQUIRE(areas_result.exit_code == 2);
    auto areas_json = parse_json_output(areas_result.stdout_output);
    REQUIRE(areas_json["ok"] == false);
    REQUIRE(areas_json["error"]["code"] == "InvalidArgument");

    auto seats_result = run_cli({"libbook", "seats", "--json"});
    REQUIRE(seats_result.exit_code == 2);
    auto seats_json = parse_json_output(seats_result.stdout_output);
    REQUIRE(seats_json["ok"] == false);
    REQUIRE(seats_json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 真实 BYXT/Score 只读命令要求显式 term", "[cli][integration][real-readonly]") {
    const std::vector<std::vector<std::string>> commands = {
        {"course", "week", "--week", "8", "--json"},
        {"week", "list", "--json"},
        {"exam", "list", "--json"},
        {"grade", "list", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        INFO(result.stdout_output);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 未知命令返回 InvalidArgument", "[cli][integration]") {
    auto result = run_cli({"unknown", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}

TEST_CASE("CLI 数字选项无效值返回 JSON InvalidArgument", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"course", "week", "--mock", "--week", "0", "--json"},
        {"course", "week", "--mock", "--week", "abc", "--json"},
        {"classroom", "query", "--mock", "--campus", "11", "--date", "2026-05-13", "--json"},
        {"classroom", "query", "--mock", "--campus", "abc", "--date", "2026-05-13", "--json"},
        {"ygdk", "records", "--mock", "--page", "0", "--json"},
        {"bykc", "courses", "--mock", "--size", "201", "--json"},
        {"bykc", "sign", "--mock", "--course-id", "bykc-1", "--sign-type", "3", "--confirm", "--json"},
        {"classroom", "query", "--mock", "--campus", "1", "--date", "2026-05-13", "--sections", "0", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 选项缺值时保留 JSON 错误合同", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"course", "week", "--week", "--json"},
        {"classroom", "free", "--campus", "--json"},
        {"course", "week", "--date", "--json"},
        {"config", "set", "--key", "--json"},
        {"config", "set", "--value", "--json"},
        {"course", "today", "--mode", "--json"},
        {"course", "week", "--term", "--json"},
        {"spoc", "assignment", "show", "--id", "--json"},
        {"bykc", "select", "--mock", "--confirm", "--course-id", "--json"},
        {"spoc", "assignment", "show", "--assignment-id", "--json"},
        {"libbook", "seats", "--area-id", "--json"},
        {"libbook", "areas", "--library-id", "--json"},
        {"libbook", "cancel", "--booking-id", "--json"},
        {"cgyy", "order", "show", "--order-id", "--json"},
        {"cgyy", "day-info", "--site-id", "--json"},
        {"ygdk", "records", "--page", "--json"},
        {"ygdk", "records", "--limit", "--json"},
        {"file", "upload", "--path", "--json"},
        {"spoc", "assignments", "--status", "--json"},
        {"spoc", "assignments", "--category", "--json"},
        {"spoc", "assignments", "--sub-category", "--json"},
        {"course", "search", "--keyword", "--json"},
        {"libbook", "book", "--start-time", "--json"},
        {"libbook", "book", "--end-time", "--json"},
        {"cgyy", "reserve", "--storey-id", "--json"},
        {"cgyy", "reserve", "--space-id", "--json"},
        {"cgyy", "reserve", "--purpose-type", "--json"},
        {"cgyy", "reserve", "--theme", "--json"},
        {"cgyy", "reserve", "--phone", "--json"},
        {"cgyy", "reserve", "--joiners", "--json"},
        {"cgyy", "reserve", "--captcha", "--json"},
        {"cgyy", "reserve", "--token", "--json"},
        {"todo", "done", "--item-id", "--json"},
        {"ygdk", "submit", "--place", "--json"},
        {"ygdk", "submit", "--photo", "--json"},
        {"libbook", "book", "--seat-id", "--json"},
        {"libbook", "book", "--segment", "--json"},
        {"course", "week", "--sections", "--json"},
        {"judge", "submit", "--input", "--json"},
        {"signin", "do", "--lat", "--json"},
        {"signin", "do", "--lng", "--json"},
        {"signin", "do", "--sign-type", "--json"},
        {"judge", "assignment", "details-batch", "--input", "--json"},
        {"config", "set", "--base-url", "--json"},
        {"config", "set", "--proxy", "--json"},
    };

    for (const auto &command : commands) {
        auto result = run_cli(command);
        REQUIRE(result.exit_code == 2);
        auto json = parse_json_output(result.stdout_output);
        require_error_envelope(json);
        CHECK(json["error"]["code"] == "InvalidArgument");
    }
}

TEST_CASE("CLI 缺少必要参数返回 InvalidArgument", "[cli][integration]") {
#if UBAANEXT_ENABLE_MOCKS
    auto result = run_cli({"login", "--mock", "--json"});
#else
    auto result = run_cli({"login", "--json"});
#endif
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}

#if !UBAANEXT_ENABLE_MOCKS
TEST_CASE("CLI Release 构建拒绝 mock 选项", "[cli][integration]") {
    auto result = run_cli({"course", "today", "--mock", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}
#endif
