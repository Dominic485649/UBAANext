/**
 * @file CookieJarTests.cpp
 * @brief CookieJar 类的单元测试
 */

#include <UBAANext/Net/CookieJar.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("CookieJar 设置和获取 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("session_id", "abc123");

    auto val = jar.get_cookie("session_id");
    REQUIRE(val.has_value());
    REQUIRE(*val == "abc123");
}

TEST_CASE("CookieJar 获取不存在的 Cookie 返回 nullopt", "[CookieJar]") {
    um::CookieJar jar;

    auto val = jar.get_cookie("missing");
    REQUIRE_FALSE(val.has_value());
}

TEST_CASE("CookieJar 移除 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("key", "value");
    jar.remove_cookie("key");

    auto val = jar.get_cookie("key");
    REQUIRE_FALSE(val.has_value());
}

TEST_CASE("CookieJar 清除所有 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("a", "1");
    jar.set_cookie("b", "2");
    jar.clear();

    REQUIRE_FALSE(jar.get_cookie("a").has_value());
    REQUIRE_FALSE(jar.get_cookie("b").has_value());
}

TEST_CASE("CookieJar 序列化为请求头", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("a", "1");
    jar.set_cookie("b", "2");

    auto header = jar.to_header();
    REQUIRE_FALSE(header.empty());
    REQUIRE(header.find("a=1") != std::string::npos);
    REQUIRE(header.find("b=2") != std::string::npos);
}

TEST_CASE("CookieJar 空罐序列化为空字符串", "[CookieJar]") {
    um::CookieJar jar;
    REQUIRE(jar.to_header().empty());
}

TEST_CASE("CookieJar 按主机隔离 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("sso.buaa.edu.cn", "SESSION", "sso");
    jar.set_cookie("byxt.buaa.edu.cn", "SESSION", "byxt");

    REQUIRE(jar.get_cookie("sso.buaa.edu.cn", "SESSION") == "sso");
    REQUIRE(jar.get_cookie("byxt.buaa.edu.cn", "SESSION") == "byxt");
    REQUIRE(jar.to_header("sso.buaa.edu.cn").find("SESSION=sso") != std::string::npos);
    REQUIRE(jar.to_header("sso.buaa.edu.cn").find("SESSION=byxt") == std::string::npos);
}

TEST_CASE("CookieJar 子域可读取父域 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("buaa.edu.cn", "sso_buaa_zhjs_token", "sso-token");

    REQUIRE(jar.get_cookie("cgyy.buaa.edu.cn", "sso_buaa_zhjs_token") == "sso-token");
}

TEST_CASE("CookieJar 序列化保留主机", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("sso.buaa.edu.cn", "SESSION", "abc");

    auto lines = jar.serialize();
    REQUIRE(lines.size() == 1);

    um::CookieJar restored;
    restored.load_serialized_line(lines[0]);
    REQUIRE(restored.get_cookie("sso.buaa.edu.cn", "SESSION") == "abc");
}

TEST_CASE("CookieJar 拒绝不安全 Cookie", "[CookieJar]") {
    um::CookieJar jar;
    jar.set_cookie("com", "PUBLIC", "bad");
    jar.set_cookie("sso.buaa.edu.cn", "BAD\r\nInjected", "bad");
    jar.set_cookie("sso.buaa.edu.cn", "TOKEN", "bad\r\nInjected: yes");
    jar.set_cookie("sso.buaa.edu.cn", "SESSION", "ok");

    REQUIRE(jar.to_header("sso.buaa.edu.cn") == "SESSION=ok");
    REQUIRE(jar.to_header("other.example.com").empty());
}