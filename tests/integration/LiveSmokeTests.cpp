#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#ifndef UBAANEXT_SOURCE_DIR
#define UBAANEXT_SOURCE_DIR ""
#endif

namespace {

[[nodiscard]] std::string get_env(const char *name) {
#if defined(_WIN32)
    char *buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return {};
    }

    std::string value(buffer);
    free(buffer);
    return value;
#else
    const char *value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

[[nodiscard]] bool env_enabled(const char *name) {
    return get_env(name) == "1";
}

[[nodiscard]] std::string shell_quote(const std::string &value) {
#if defined(_WIN32)
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
#endif
}

[[nodiscard]] std::string powershell_executable() {
#if defined(_WIN32)
    return "powershell";
#else
    if (!get_env("UBAANEXT_POWERSHELL").empty()) return get_env("UBAANEXT_POWERSHELL");
    return "pwsh";
#endif
}

[[nodiscard]] std::string run_command_capture(const std::string &command, int &exit_code) {
    std::array<char, 4096> buffer{};
    std::string output;
#if defined(_WIN32)
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(_popen((command + " 2>&1").c_str(), "r"), _pclose);
#else
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(popen((command + " 2>&1").c_str(), "r"), pclose);
#endif
    if (!pipe) {
        exit_code = -1;
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        output += buffer.data();
    }
#if defined(_WIN32)
    exit_code = _pclose(pipe.release());
#else
    exit_code = pclose(pipe.release());
#endif
    return output;
}

[[nodiscard]] std::string read_text(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::string source_dir() {
    const auto from_env = get_env("UBAANEXT_SOURCE_DIR");
    return from_env.empty() ? std::string{UBAANEXT_SOURCE_DIR} : from_env;
}

[[nodiscard]] bool powershell_available() {
    int exit_code = 0;
    run_command_capture(powershell_executable() + " -NoProfile -Command \"exit 0\"", exit_code);
    return exit_code == 0;
}

} // namespace

TEST_CASE("Live smoke tests are disabled by default", "[live][smoke]") {
    REQUIRE_FALSE(env_enabled("UBAANEXT_LIVE"));
}

TEST_CASE("Live smoke runner requires explicit write gates", "[live][smoke]") {
    if (!env_enabled("UBAANEXT_LIVE")) {
        SUCCEED("live smoke is disabled");
        return;
    }

    REQUIRE_FALSE(env_enabled("UBAANEXT_ALLOW_WRITE"));
}

TEST_CASE("Live smoke script keeps reversible cloud writes behind three gates", "[live][smoke][security]") {
    const auto source_dir_value = source_dir();
    REQUIRE_FALSE(source_dir_value.empty());
    const auto script = std::filesystem::path(source_dir_value) / "tools" / "live-smoke.ps1";
    REQUIRE(std::filesystem::exists(script));
    const auto text = read_text(script);

    CHECK(text.find("UBAANEXT_CLOUD_WRITE -eq '1'") != std::string::npos);
    CHECK(text.find("UBAANEXT_ALLOW_WRITE -ne '1' -or $env:UBAANEXT_CONFIRM_WRITE -ne '1'") != std::string::npos);
    CHECK(text.find("file', 'mkdir'") != std::string::npos);
    CHECK(text.find("file', 'upload'") != std::string::npos);
    CHECK(text.find("file', 'delete'") != std::string::npos);
    CHECK(text.find("file', 'recycle-delete'") != std::string::npos);
    CHECK(text.find("UBAANEXT_CLOUD_WRITE_CLEAN_RECYCLE -eq '1'") != std::string::npos);
}

TEST_CASE("Live smoke runner redacts credentials from command trace", "[live][smoke]") {
    const auto username = get_env("UBAANEXT_USERNAME");
    const auto password = get_env("UBAANEXT_PASSWORD");
    if (!env_enabled("UBAANEXT_LIVE") || username.empty() || password.empty()) {
        SUCCEED("live smoke credentials are unavailable");
        return;
    }

    const auto source_dir_value = source_dir();
    REQUIRE_FALSE(source_dir_value.empty());
    const auto script = std::filesystem::path(source_dir_value) / "tools" / "live-smoke.ps1";
    REQUIRE(std::filesystem::exists(script));
    if (!powershell_available()) {
        SUCCEED("PowerShell is unavailable");
        return;
    }

    int exit_code = 0;
    auto output = run_command_capture(powershell_executable() + " -NoProfile -ExecutionPolicy Bypass -File " + shell_quote(script.string()) +
                                          " -CliPath " + shell_quote("Z:/missing/ubaa.exe"),
                                      exit_code);

    CHECK(exit_code != 0);
    CHECK(output.find(username) == std::string::npos);
    CHECK(output.find(password) == std::string::npos);
}

TEST_CASE("Live smoke readonly runner", "[live][smoke][readonly]") {
    if (!env_enabled("UBAANEXT_LIVE")) {
        SUCCEED("live smoke is disabled");
        return;
    }

    REQUIRE_FALSE(get_env("UBAANEXT_USERNAME").empty());
    REQUIRE_FALSE(get_env("UBAANEXT_PASSWORD").empty());
    REQUIRE_FALSE(env_enabled("UBAANEXT_ALLOW_WRITE"));

    const auto source_dir_value = source_dir();
    REQUIRE_FALSE(source_dir_value.empty());
    const auto script = std::filesystem::path(source_dir_value) / "tools" / "live-smoke.ps1";
    REQUIRE(std::filesystem::exists(script));

    const auto cli_path = get_env("UBAANEXT_CLI_PATH").empty() ? get_env("UBAA_CLI_PATH") : get_env("UBAANEXT_CLI_PATH");
    REQUIRE_FALSE(cli_path.empty());
    REQUIRE(std::filesystem::exists(cli_path));
    if (!powershell_available()) {
        SUCCEED("PowerShell is unavailable");
        return;
    }

    int exit_code = 0;
    auto output = run_command_capture(powershell_executable() + " -NoProfile -ExecutionPolicy Bypass -File " + shell_quote(script.string()) +
                                          " -Level L1 -CliPath " + shell_quote(cli_path),
                                      exit_code);

    INFO(output);
    CHECK(exit_code == 0);
}
