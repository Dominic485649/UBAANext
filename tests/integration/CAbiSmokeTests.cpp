#include <catch2/catch_test_macros.hpp>

#include <UBAANext/Bindings/C/UbaaNative.h>

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

class NativeJsonResult {
public:
    explicit NativeJsonResult(const char *value) : m_value(value) {}
    NativeJsonResult(const NativeJsonResult &) = delete;
    NativeJsonResult &operator=(const NativeJsonResult &) = delete;

    ~NativeJsonResult() {
        ubaanext_release_result(m_value);
    }

    [[nodiscard]] const char *get() const {
        return m_value;
    }

    [[nodiscard]] nlohmann::json parse() const {
        REQUIRE(m_value != nullptr);
        return nlohmann::json::parse(m_value);
    }

private:
    const char *m_value = nullptr;
};

void require_success_envelope(const nlohmann::json &json) {
    REQUIRE(json["ok"] == true);
    REQUIRE(json.contains("data"));
    REQUIRE(json["data"].is_object());
    REQUIRE(json.contains("error"));
    REQUIRE(json["error"].is_null());
}

void require_error_envelope(const nlohmann::json &json) {
    REQUIRE(json["ok"] == false);
    REQUIRE(json.contains("data"));
    REQUIRE(json["data"].is_null());
    REQUIRE(json.contains("error"));
    REQUIRE(json["error"].is_object());
    REQUIRE(json["error"].contains("code"));
    REQUIRE(json["error"].contains("message"));
}

} // namespace

TEST_CASE("C ABI version and capability smoke", "[cabi][integration]") {
    const char *version = ubaanext_version();
    REQUIRE(version != nullptr);
    CHECK(std::string{version}.empty() == false);

    CHECK(ubaanext_get_capabilities(nullptr) == UBAANEXT_STATUS_INVALID_ARGUMENT);

    UbaaNextCapabilities capabilities{};
    std::memset(&capabilities, 0xCC, sizeof(capabilities));
    REQUIRE(ubaanext_get_capabilities(&capabilities) == UBAANEXT_STATUS_OK);

    const auto *reserved = reinterpret_cast<const std::uint8_t *>(capabilities.reserved);
    for (std::size_t index = 0; index < sizeof(capabilities.reserved); ++index) {
        INFO("reserved byte index: " << index);
        CHECK(reserved[index] == 0U);
    }
}

TEST_CASE("C ABI context mode and mock readonly smoke", "[cabi][integration]") {
    CHECK(ubaanext_context_set_connection_mode(nullptr, "mock") == UBAANEXT_STATUS_INVALID_ARGUMENT);

    UbaaNextContext *context = ubaanext_context_create();
    REQUIRE(context != nullptr);

    CHECK(ubaanext_context_set_connection_mode(context, "not-a-mode") == UBAANEXT_STATUS_INVALID_CONNECTION_MODE);
#if UBAANEXT_ENABLE_MOCKS
    REQUIRE(ubaanext_context_set_connection_mode(context, "mock") == UBAANEXT_STATUS_OK);

    NativeJsonResult result{ubaanext_terms(context)};
    const auto json = result.parse();
    require_success_envelope(json);
    REQUIRE(json["data"].contains("terms"));
    REQUIRE(json["data"]["terms"].is_array());
    REQUIRE_FALSE(json["data"]["terms"].empty());
#else
    CHECK(ubaanext_context_set_connection_mode(context, "mock") == UBAANEXT_STATUS_INVALID_CONNECTION_MODE);
#endif

    ubaanext_context_release(context);
}

TEST_CASE("C ABI JSON failures keep envelope contract", "[cabi][integration]") {
    NativeJsonResult result{ubaanext_terms(nullptr)};
    const auto json = result.parse();
    require_error_envelope(json);
    CHECK(json["error"]["code"] == "InvalidArgument");
}
