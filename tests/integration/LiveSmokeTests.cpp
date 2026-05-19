#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <string>

namespace {

[[nodiscard]] std::string get_env(const char *name) {
    char *buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || buffer == nullptr) {
        return {};
    }

    std::string value(buffer);
    free(buffer);
    return value;
}

[[nodiscard]] bool env_enabled(const char *name) {
    return get_env(name) == "1";
}

} // namespace

TEST_CASE("Live smoke tests are disabled by default", "[live][smoke]") {
    REQUIRE_FALSE(env_enabled("UBAANEXT_LIVE"));
}

TEST_CASE("Live smoke runner requires explicit write gates", "[live][smoke]") {
    if (!env_enabled("UBAANEXT_LIVE")) {
        SUCCEED("live smoke is disabled");
        return;
    }

    REQUIRE_FALSE(env_enabled("UBAANEXT_ALLOW_WRITE"));
}
