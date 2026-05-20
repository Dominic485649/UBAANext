#pragma once

#include "AppContext.hpp"
#include "CliConfig.hpp"

#include <filesystem>

namespace UBAANextCli {

struct PlatformContextOptions {
    bool mock = false;
    std::string mode;
    CliConfig config;
    std::filesystem::path session_file_path;
    std::filesystem::path cookie_file_path;
};

[[nodiscard]] AppContext create_current_platform_context(const PlatformContextOptions &options);
void save_platform_cookies(AppContext &ctx);
void clear_platform_cookies(AppContext &ctx);

} // namespace UBAANextCli
