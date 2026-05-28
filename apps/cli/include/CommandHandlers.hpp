#pragma once

#include <string>

namespace UBAANextCli {

class OutputFormatter;

void print_usage();
void print_help(OutputFormatter &out);

[[nodiscard]] bool is_cli_command(const std::string &command);
[[nodiscard]] bool is_command_with_action(const std::string &command);

} // namespace UBAANextCli
