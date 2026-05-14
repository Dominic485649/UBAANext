/**
 * @file CliConfig.cpp
 * @brief CLI 配置的 JSON 序列化实现
 */

#include "CliConfig.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace UBAANextCli {

void CliConfig::save(const std::string &path) const {
    nlohmann::json j = {
        {"mode",         mode},
        {"proxy",        proxy},
        {"cacheEnabled", cache_enabled},
    };

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(2) << '\n';
    }
}

CliConfig CliConfig::load(const std::string &path) {
    CliConfig config;

    if (!std::filesystem::exists(path)) {
        return config;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return config;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(ifs);

        if (j.contains("mode") && j["mode"].is_string()) {
            config.mode = j["mode"].get<std::string>();
        }
        if (j.contains("proxy") && j["proxy"].is_string()) {
            config.proxy = j["proxy"].get<std::string>();
        }
        if (j.contains("cacheEnabled") && j["cacheEnabled"].is_boolean()) {
            config.cache_enabled = j["cacheEnabled"].get<bool>();
        }
    } catch (const nlohmann::json::exception &) {
        // 解析失败，返回默认配置
    }

    return config;
}

} // namespace UBAANextCli
