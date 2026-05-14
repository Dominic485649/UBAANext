#include <UBAANext/Net/VpnCipher.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("VpnCipher 主机加密与 UBAA WebVPN 实现一致", "[VpnCipher]") {
    REQUIRE(um::VpnCipher::to_vpn_url("https://byxt.buaa.edu.cn/jwapp/sys/homeapp/index.html") ==
            "https://d.buaa.edu.cn/https/77726476706e69737468656265737421f2ee598869327d517f468ca88d1b203b/jwapp/sys/homeapp/index.html");
    REQUIRE(um::VpnCipher::to_vpn_url("https://sso.buaa.edu.cn/login") ==
            "https://d.buaa.edu.cn/https/77726476706e69737468656265737421e3e44ed225256951300d8db9d6562d/login");
}

TEST_CASE("VpnCipher 保留端口、查询参数和片段", "[VpnCipher]") {
    REQUIRE(um::VpnCipher::to_vpn_url("https://iclass.buaa.edu.cn:8347/app/user/login.action?x=1#top") ==
            "https://d.buaa.edu.cn/https-8347/77726476706e69737468656265737421f9f44d9d342326526b0988e29d51367ba018/app/user/login.action?x=1#top");
    REQUIRE(um::VpnCipher::to_vpn_url("https://d.buaa.edu.cn/https/abc/path") ==
            "https://d.buaa.edu.cn/https/abc/path");
}
