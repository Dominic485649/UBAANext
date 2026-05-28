/**
 * @file VpnCipher.hpp
 * @brief BUAA WebVPN URL 加密工具
 *
 * 将内网 URL 转换为 WebVPN 网关 URL。
 * 加密算法: AES/CFB/NoPadding，key 和 IV 均为 "wrdvpnisthebest!"
 *
 * WebVPN 主机名转换沿用 BUAA WebVPN 的 AES/CFB/NoPadding 约定。
 */
#pragma once

#include <string>

namespace UBAANext {

/**
 * @brief WebVPN URL 加密/转换工具
 *
 * 将 https://sso.buaa.edu.cn/login
 * 转换为 https://d.buaa.edu.cn/https/<encrypted-host>/login
 */
class VpnCipher {
public:
    /**
     * @brief PartiallyMigrated WebVPN URL converter; output may contain encoded host/path and must keep query strings redacted in diagnostics.
     * @param url 原始内网 URL（如 https://sso.buaa.edu.cn/login）
     * @return WebVPN URL（如 https://d.buaa.edu.cn/https/<enc>/login）
     */
    [[nodiscard]] static std::string to_vpn_url(const std::string &url);
    [[nodiscard]] static std::string from_vpn_url(const std::string &url);

    /**
     * @brief Experimental reachability probe; may perform a network check and must not be used as semantic completion proof.
     * @return true 表示可以直连，false 表示需要 VPN
     */
    [[nodiscard]] static bool can_direct_connect();

private:
    /**
     * @brief Crypto boundary: AES/CFB encrypts WebVPN host names; UnsupportedCrypto must fail closed.
     * @param host 原始主机名（如 sso.buaa.edu.cn）
     * @return 加密后的十六进制字符串
     */
    [[nodiscard]] static std::string encrypt_host(const std::string &host);
};

} // namespace UBAANext
