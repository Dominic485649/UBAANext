#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace {

[[nodiscard]] std::string get_env(const char *name) {
    char *buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return {};
    }

    std::string value(buffer);
    free(buffer);
    return value;
}

[[nodiscard]] bool env_enabled(const char *name) {
    return get_env(name) == "1";
}

[[nodiscard]] std::string shell_quote(const std::string &value) {
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
}

[[nodiscard]] std::string run_command_capture(const std::string &command, int &exit_code) {
    std::array<char, 4096> buffer{};
    std::string output;
    std::unique_ptr<FILE, int (*)(FILE *)> pipe(_popen((command + " 2>&1").c_str(), "r"), _pclose);
    if (!pipe) {
        exit_code = -1;
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        output += buffer.data();
    }
    exit_code = _pclose(pipe.release());
    return output;
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

TEST_CASE("Live smoke runner redacts credentials from command trace", "[live][smoke]") {
    const auto username = get_env("UBAANEXT_USERNAME");
    const auto password = get_env("UBAANEXT_PASSWORD");
    if (!env_enabled("UBAANEXT_LIVE") || username.empty() || password.empty()) {
        SUCCEED("live smoke credentials are unavailable");
        return;
    }

    const auto source_dir = get_env("UBAANEXT_SOURCE_DIR");
    REQUIRE_FALSE(source_dir.empty());
    const auto script = std::filesystem::path(source_dir) / "tools" / "live-smoke.ps1";
    REQUIRE(std::filesystem::exists(script));

    int exit_code = 0;
    auto output = run_command_capture("powershell -NoProfile -ExecutionPolicy Bypass -File " + shell_quote(script.string()) +
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

    const auto source_dir = get_env("UBAANEXT_SOURCE_DIR");
    REQUIRE_FALSE(source_dir.empty());
    const auto script = std::filesystem::path(source_dir) / "tools" / "live-smoke.ps1";
    REQUIRE(std::filesystem::exists(script));

    const auto cli_path = get_env("UBAANEXT_CLI_PATH").empty() ? get_env("UBAA_CLI_PATH") : get_env("UBAANEXT_CLI_PATH");
    REQUIRE_FALSE(cli_path.empty());
    REQUIRE(std::filesystem::exists(cli_path));

    int exit_code = 0;
    auto output = run_command_capture("powershell -NoProfile -ExecutionPolicy Bypass -File " + shell_quote(script.string()) +
                                          " -Level L1 -CliPath " + shell_quote(cli_path),
                                      exit_code);

    INFO(output);
    CHECK(exit_code == 0);
}
