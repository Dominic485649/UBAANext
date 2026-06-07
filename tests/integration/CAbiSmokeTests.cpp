#include <catch2/catch_test_macros.hpp>

#include <UBAANext/Bindings/C/UbaaNative.h>

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
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

void set_env_var(const char *name, const std::filesystem::path &value) {
#ifdef _WIN32
    REQUIRE(_putenv_s(name, value.string().c_str()) == 0);
#else
    REQUIRE(setenv(name, value.string().c_str(), 1) == 0);
#endif
}

void configure_td_app_data_for_test() {
    const auto root = std::filesystem::temp_directory_path() / "ubaanext-cabi-smoke";
#if defined(__OHOS__)
    set_env_var("UBAANEXT_HARMONY_APPDATA", root / "harmony-appdata");
#elif defined(_WIN32)
    set_env_var("LOCALAPPDATA", root / "localappdata");
#else
    set_env_var("XDG_DATA_HOME", root / "xdg-data");
#endif
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

    NativeJsonResult capabilities_json{ubaanext_capabilities()};
    const auto cap_json = capabilities_json.parse();
    require_success_envelope(cap_json);
    REQUIRE(cap_json["data"].contains("capabilities"));
    REQUIRE(cap_json["data"]["capabilities"].contains("realNetwork"));

    NativeJsonResult version_json{ubaanext_version_info()};
    const auto version_info = version_json.parse();
    require_success_envelope(version_info);
    CHECK(version_info["data"]["version"].get<std::string>().empty() == false);
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

    NativeJsonResult login{ubaanext_auth_login(context, "mock-user", "mock-password", nullptr)};
    const auto login_json = login.parse();
    require_success_envelope(login_json);
    REQUIRE(login_json["data"].contains("account"));
    CHECK(login_json["data"]["account"]["studentId"] == "mock-user");

    NativeJsonResult whoami{ubaanext_auth_whoami(context)};
    const auto whoami_json = whoami.parse();
    require_success_envelope(whoami_json);
    CHECK(whoami_json["data"]["active"] == true);
    CHECK(whoami_json["data"]["mode"] == "mock");
    CHECK(whoami_json["data"]["account"]["studentId"] == "mock-user");

    NativeJsonResult todos{ubaanext_todos(context, 1)};
    const auto todos_json = todos.parse();
    require_success_envelope(todos_json);
    REQUIRE(todos_json["data"].contains("todos"));
    REQUIRE(todos_json["data"]["todos"].is_array());

    NativeJsonResult evaluation{ubaanext_feature_list(context, "evaluation", "list")};
    const auto evaluation_json = evaluation.parse();
    require_success_envelope(evaluation_json);
    REQUIRE(evaluation_json["data"].contains("features"));
    REQUIRE(evaluation_json["data"]["features"].is_array());

    NativeJsonResult spoc{ubaanext_feature_list(context, "spoc", "assignments")};
    const auto spoc_json = spoc.parse();
    require_success_envelope(spoc_json);
    REQUIRE(spoc_json["data"].contains("features"));
    REQUIRE(spoc_json["data"]["features"].is_array());

    NativeJsonResult judge{ubaanext_feature_list(context, "judge", "assignments")};
    const auto judge_json = judge.parse();
    require_success_envelope(judge_json);
    REQUIRE(judge_json["data"].contains("features"));
    REQUIRE(judge_json["data"]["features"].is_array());

    NativeJsonResult bykc{ubaanext_feature_list(context, "bykc", "courses")};
    const auto bykc_json = bykc.parse();
    require_success_envelope(bykc_json);
    REQUIRE(bykc_json["data"].contains("features"));
    REQUIRE(bykc_json["data"]["features"].is_array());

    NativeJsonResult cgyy{ubaanext_feature_list(context, "cgyy", "sites")};
    const auto cgyy_json = cgyy.parse();
    require_success_envelope(cgyy_json);
    REQUIRE(cgyy_json["data"].contains("features"));
    REQUIRE(cgyy_json["data"]["features"].is_array());

    NativeJsonResult libbook{ubaanext_feature_list(context, "libbook", "libraries")};
    const auto libbook_json = libbook.parse();
    require_success_envelope(libbook_json);
    REQUIRE(libbook_json["data"].contains("features"));
    REQUIRE(libbook_json["data"]["features"].is_array());

    NativeJsonResult signin_write{ubaanext_signin_do(context, "mock-course", 0)};
    const auto signin_write_json = signin_write.parse();
    require_error_envelope(signin_write_json);
    CHECK(signin_write_json["error"]["code"] == "InvalidArgument");
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

    UbaaNextContext *context = ubaanext_context_create();
    REQUIRE(context != nullptr);

    NativeJsonResult feature_list{ubaanext_feature_list(context, nullptr, "list")};
    const auto feature_list_json = feature_list.parse();
    require_error_envelope(feature_list_json);
    CHECK(feature_list_json["error"]["code"] == "InvalidArgument");

    NativeJsonResult feature_show{ubaanext_feature_show(context, "spoc", "assignment", nullptr)};
    const auto feature_show_json = feature_show.parse();
    require_error_envelope(feature_show_json);
    CHECK(feature_show_json["error"]["code"] == "InvalidArgument");

    ubaanext_context_release(context);
}

TEST_CASE("C ABI TD local readonly endpoints keep envelope contract", "[cabi][integration]") {
    configure_td_app_data_for_test();

    UbaaNextContext *context = ubaanext_context_create();
    REQUIRE(context != nullptr);

    NativeJsonResult status{ubaanext_td_status(context)};
    const auto status_json = status.parse();
    require_success_envelope(status_json);
    REQUIRE(status_json["data"].contains("tdStates"));
    REQUIRE(status_json["data"]["tdStates"].is_array());

    NativeJsonResult users{ubaanext_td_users(context)};
    const auto users_json = users.parse();
    require_success_envelope(users_json);
    REQUIRE(users_json["data"].contains("tdUsers"));
    REQUIRE(users_json["data"]["tdUsers"].is_array());

    NativeJsonResult counts{ubaanext_td_count_cache(context, nullptr)};
    const auto counts_json = counts.parse();
    require_success_envelope(counts_json);
    REQUIRE(counts_json["data"].contains("tdCounts"));
    REQUIRE(counts_json["data"]["tdCounts"].is_array());

    ubaanext_context_release(context);
}
