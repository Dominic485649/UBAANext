#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Platform/PlatformCapabilities.hpp>

#if defined(UBAANEXT_TEST_HARMONY_TYPES)
#include <UBAANext/Platform/Harmony/HarmonyAppDataPathProvider.hpp>
#include <UBAANext/Platform/Harmony/UnsupportedSecureStore.hpp>
#endif

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

namespace {

UBAANext::PlatformCapabilities planned_harmony_capabilities() {
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

TEST_CASE("Harmony skeleton capabilities keep secure persistence unsupported", "[platform][harmony]") {
    const auto caps = planned_harmony_capabilities();

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

#if defined(UBAANEXT_TEST_HARMONY_TYPES)
TEST_CASE("Harmony secure store fails fast without HUKS adapter", "[platform][harmony]") {
    UBAANext::Platform::Harmony::UnsupportedSecureStore store;

    CHECK_FALSE(store.get_string("cookies.v1").has_value());
    CHECK_THROWS_AS(store.set_string("cookies.v1", "secret"), std::runtime_error);
    CHECK_THROWS_AS(store.remove("cookies.v1"), std::runtime_error);
    CHECK_THROWS_AS(store.clear(), std::runtime_error);
}

TEST_CASE("Harmony app data path requires shell provided sandbox path", "[platform][harmony]") {
    UBAANext::Platform::Harmony::HarmonyAppDataPathProvider provider;

    auto path = provider.app_data_dir();
    if (!path) {
        CHECK(path.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    } else {
        CHECK_FALSE(path->empty());
    }
}
#endif
