#pragma once

#include <string>
#include <vector>

namespace UBAANextCli {

struct CliCommandResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

[[nodiscard]] CliCommandResult run_cli_command(const std::vector<std::string> &arguments);

} // namespace UBAANextCli
