#include "CliRunner.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
    auto result = UBAANextCli::run_cli_command(std::vector<std::string>(argv + 1, argv + argc));
    std::cout << result.stdout_text;
    std::cerr << result.stderr_text;
    return result.exit_code;
}
