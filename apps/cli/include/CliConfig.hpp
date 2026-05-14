/**
 * @file CliConfig.hpp
 * @brief CLI 配置管理
 */
#pragma once

#include <string>

namespace UBAANextCli {

struct CliConfig {
    std::string mode          = "vpn";
    std::string proxy;
    bool        cache_enabled = true;

    void save(const std::string &path) const;
    static CliConfig load(const std::string &path);
};

} // namespace UBAANextCli
