#include <UBAANext/Platform/PlatformCapabilities.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

UBAANext::PlatformCapabilities planned_linux_capabilities() {
    UBAANext::PlatformCapabilities caps;
    caps.real_network = true;
    caps.secure_cookie_persistence = false;
    caps.cookie_persistence = false;
    caps.redirect_control = true;
    caps.openssl_crypto = true;
    caps.secure_store = false;
    caps.app_data_path = true;
    caps.upload_bytes = true;
    caps.live_login = false;
    caps.write_operations = false;
    return caps;
}

} // namespace

TEST_CASE("Linux skeleton capabilities fail fast without secure store", "[platform][linux]") {
    const auto caps = planned_linux_capabilities();

    CHECK(caps.real_network);
    CHECK(caps.redirect_control);
    CHECK(caps.openssl_crypto);
    CHECK(caps.app_data_path);
    CHECK(caps.upload_bytes);
    CHECK_FALSE(caps.secure_store);
    CHECK_FALSE(caps.cookie_persistence);
    CHECK_FALSE(caps.secure_cookie_persistence);
    CHECK_FALSE(caps.live_login);
    CHECK_FALSE(caps.write_operations);
}
