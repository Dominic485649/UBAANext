/**
 * @file CliIntegrationTests.cpp
 * @brief CLI 集成测试
 *
 * 测试 CLI 命令的 JSON 输出格式和 exit code。
 * 使用 --mock 模式避免真实网络调用。
 */

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

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
    auto result = run_cli({"help", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("commands"));
    REQUIRE(json["data"].contains("version"));
    REQUIRE(json["data"]["commands"].is_array());
    REQUIRE_FALSE(json["data"]["commands"].empty());
}

#if UBAANEXT_ENABLE_MOCKS
TEST_CASE("CLI login mock 命令", "[cli][integration]") {
    auto result = run_cli({"login", "--mock", "--username", "20260000", "--password", "test", "--json"});
    REQUIRE(result.exit_code == 0);

    // 登录命令输出两个 JSON 对象（多行），检查包含关键字段
    REQUIRE(result.stdout_output.find("\"ok\": true") != std::string::npos);
    REQUIRE(result.stdout_output.find("\"studentId\"") != std::string::npos);
    REQUIRE(result.stdout_output.find("20260000") != std::string::npos);
    REQUIRE(result.stdout_output.find("登录成功") != std::string::npos);
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
        {"evaluation", "list", "--mock", "--json"},
        {"todo", "list", "--mock", "--json"},
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
        {"grade", "all", "--json"},
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
