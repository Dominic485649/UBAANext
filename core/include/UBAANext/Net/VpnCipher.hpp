/**
 * @file VpnCipher.hpp
 * @brief BUAA WebVPN URL 加密工具
 *
 * 将内网 URL 转换为 WebVPN 网关 URL。
 * 加密算法: AES/CFB/NoPadding，key 和 IV 均为 "wrdvpnisthebest!"
 *
 * 参考: reference/UBAA/shared/.../LocalWebVpnSupport.kt
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
     * @brief 将内网 URL 转换为 WebVPN URL
     * @param url 原始内网 URL（如 https://sso.buaa.edu.cn/login）
     * @return WebVPN URL（如 https://d.buaa.edu.cn/https/<enc>/login）
     */
    [[nodiscard]] static std::string to_vpn_url(const std::string &url);

    /**
     * @brief 检测是否能直连内网
     *
     * 尝试 HEAD 请求 byxt.buaa.edu.cn，如果失败则需要 VPN。
     * @return true 表示可以直连，false 表示需要 VPN
     */
    [[nodiscard]] static bool can_direct_connect();

private:
    /**
     * @brief AES/CFB 加密主机名
     * @param host 原始主机名（如 sso.buaa.edu.cn）
     * @return 加密后的十六进制字符串
     */
    [[nodiscard]] static std::string encrypt_host(const std::string &host);
};

} // namespace UBAANext
