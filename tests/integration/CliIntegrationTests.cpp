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
    std::string cmd = "\"";
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

    auto result = run_cli({"logout", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("message"));
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

TEST_CASE("CLI cache clear 命令", "[cli][integration]") {
    auto result = run_cli({"cache", "clear", "--json"});
    REQUIRE(result.exit_code == 0);

    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == true);
    REQUIRE(json["data"].contains("message"));
}

TEST_CASE("CLI 新增只读命令 mock smoke", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"user", "info", "--mock", "--json"},
        {"app", "announcement", "--mock", "--json"},
        {"grade", "list", "--mock", "--term", "2025-2026-2", "--json"},
        {"spoc", "assignments", "--mock", "--json"},
        {"spoc", "assignment", "show", "--mock", "--id", "spoc-1", "--json"},
        {"judge", "assignments", "--mock", "--course-id", "course-1", "--json"},
        {"judge", "assignment", "show", "--mock", "--assignment-id", "judge-1", "--json"},
        {"judge", "assignment", "details", "--mock", "--assignment-id", "judge-1", "--json"},
        {"signin", "today", "--mock", "--json"},
        {"ygdk", "overview", "--mock", "--json"},
        {"ygdk", "records", "--mock", "--json"},
        {"evaluation", "list", "--mock", "--json"},
        {"bykc", "profile", "--mock", "--json"},
        {"bykc", "courses", "--mock", "--json"},
        {"bykc", "chosen", "--mock", "--json"},
        {"bykc", "course", "show", "--mock", "--course-id", "bykc-1", "--json"},
        {"bykc", "stats", "--mock", "--json"},
        {"cgyy", "sites", "--mock", "--json"},
        {"cgyy", "purpose-types", "--mock", "--json"},
        {"cgyy", "day-info", "--mock", "--json"},
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
    auto result = run_cli({"signin", "do", "--mock", "--json"});
    REQUIRE(result.exit_code == 2);
    auto json = parse_json_output(result.stdout_output);
    REQUIRE(json["ok"] == false);
}

TEST_CASE("CLI 有副作用命令 confirm 后 mock 可执行", "[cli][integration]") {
    const std::vector<std::vector<std::string>> commands = {
        {"signin", "do", "--mock", "--confirm", "--json"},
        {"ygdk", "submit", "--mock", "--confirm", "--json"},
        {"evaluation", "submit", "--mock", "--confirm", "--json"},
        {"bykc", "select", "--mock", "--course-id", "bykc-1", "--confirm", "--json"},
        {"bykc", "unselect", "--mock", "--course-id", "bykc-1", "--confirm", "--json"},
        {"bykc", "sign", "--mock", "--course-id", "bykc-1", "--confirm", "--json"},
        {"cgyy", "reserve", "--mock", "--confirm", "--json"},
        {"cgyy", "order", "cancel", "--mock", "--order-id", "cgyy-1", "--confirm", "--json"},
        {"libbook", "book", "--mock", "--area-id", "libbook-1", "--confirm", "--json"},
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

TEST_CASE("CLI 未知命令返回 InvalidArgument", "[cli][integration]") {
    auto result = run_cli({"unknown", "--json"});
    REQUIRE(result.exit_code == 2);  // InvalidArgument
}

TEST_CASE("CLI 缺少必要参数返回 InvalidArgument", "[cli][integration]") {
    auto result = run_cli({"login", "--mock", "--json"});
    REQUIRE(result.exit_code == 2);  // InvalidArgument
}
