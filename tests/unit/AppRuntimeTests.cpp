#include <UBAANext/Runtime/AppRuntime.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace runtime = UBAANext::Runtime;

namespace {

std::filesystem::path test_app_dir(const char *name) {
    auto dir = std::filesystem::temp_directory_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

UBAANext::Model::CloudItem runtime_root_item() {
    UBAANext::Model::CloudItem item;
    item.id = "root";
    item.name = "Cloud";
    item.type = "doc_lib";
    return item;
}

} // namespace

TEST_CASE("RuntimeContext exposes mock session and diagnostics", "[Runtime]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-session-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    runtime::LoginRequest request;
    request.username = "20260000";
    request.password = "test";
    auto login = ctx.login(request);
    REQUIRE(login.has_value());

    auto account = ctx.whoami();
    REQUIRE(account.has_value());
    CHECK(account->student_id == "20260000");

    auto diagnostics = ctx.diagnostics();
    CHECK(diagnostics.mock);
    CHECK(diagnostics.session_present);
    CHECK_FALSE(diagnostics.account_summary.find("20260000") != std::string::npos);

    auto json = ctx.diagnostics_json();
    REQUIRE(json.has_value());
    CHECK(json->find("sessionPresent") != std::string::npos);
    CHECK(json->find("20260000") == std::string::npos);
    CHECK(json->find("test") == std::string::npos);
    auto parsed = nlohmann::json::parse(*json);
    CHECK(parsed["account"]["sessionPresent"] == true);
    CHECK(parsed["account"]["summary"].get<std::string>().find("[REDACTED]") != std::string::npos);
    CHECK(parsed["paths"].contains("configFile"));
    CHECK(parsed["paths"].contains("sessionFile"));
    CHECK(parsed["paths"].contains("cookieFile"));
    CHECK(parsed["paths"].contains("cacheDir"));
    CHECK(parsed["paths"].contains("logDir"));
    CHECK(parsed["session"]["present"] == true);
    CHECK(parsed["session"].contains("storageAvailable"));
    CHECK(parsed["cache"].contains("enabled"));
    CHECK(parsed["cache"].contains("dir"));
    CHECK(parsed["dependencies"]["mounts"].contains("winfsp"));
    CHECK(parsed["dependencies"]["mounts"].contains("cloudFiles"));
    CHECK(parsed["dependencies"]["mounts"].contains("fuse"));

    auto logout = ctx.logout();
    REQUIRE(logout.has_value());
    CHECK_FALSE(ctx.whoami().has_value());
}

TEST_CASE("RuntimeContext clears filesystem cache after confirmation boundary caller", "[Runtime]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-cache-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    std::filesystem::create_directories(ctx.cache_dir());
    {
        std::ofstream file(ctx.cache_dir() / "payload.bin", std::ios::binary);
        file << "payload";
    }
    CHECK(ctx.cache_size_bytes() > 0);

    const bool unconfirmed = false;
    auto denied = ctx.clear_cache(unconfirmed);
    REQUIRE_FALSE(denied);
    CHECK(ctx.cache_size_bytes() > 0);

    const bool confirmed = true;
    auto cleared = ctx.clear_cache(confirmed);
    REQUIRE(cleared.has_value());
    CHECK(ctx.cache_size_bytes() == 0);
}

TEST_CASE("RuntimeContext exposes typed feature facade", "[Runtime][feature]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-feature-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    runtime::RuntimeFeatureQuery query;
    query.domain = "signin";
    query.operation = "today";
    auto state = ctx.feature_list(query);
    REQUIRE(state.has_value());
    CHECK(state->title == "signin today");
    REQUIRE_FALSE(state->rows.empty());
    CHECK(state->rows.front().id == "signin-today");

    query.operation = "do";
    auto denied = ctx.feature_mutate(query);
    REQUIRE_FALSE(denied);
    CHECK(denied.error().message.find("显式确认") != std::string::npos);
}

TEST_CASE("RuntimeContext cloud path traversal fails closed before remote access", "[Runtime][cloud]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-cloud-browser-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    auto rejected = ctx.cloud_open_path("/../secret", "");
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(rejected.error().message.find("..") != std::string::npos);
    CHECK(ctx.cloud_state().current_path == "/");
}

TEST_CASE("RuntimeContext cloud write operations require confirmation", "[Runtime][cloud]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-cloud-write-confirm-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);
    REQUIRE(ctx.cloud_vfs().set_root(runtime_root_item()));

    auto denied_mkdir = ctx.cloud_create_dir("denied", false);
    REQUIRE_FALSE(denied_mkdir);
    CHECK(denied_mkdir.error().message.find("显式确认") != std::string::npos);
    auto mkdir_tasks = ctx.cloud_state().tasks;
    REQUIRE(mkdir_tasks.size() == 1);
    CHECK(mkdir_tasks.front().operation == "mkdir");
    CHECK(mkdir_tasks.front().status == "failed");
}

TEST_CASE("RuntimeContext cloud upload requires confirmation", "[Runtime][cloud]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-cloud-upload-confirm-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);
    REQUIRE(ctx.cloud_vfs().set_root(runtime_root_item()));

    auto upload_path = options.app_data_dir / "upload.txt";
    {
        std::ofstream file(upload_path, std::ios::binary);
        file << "hello";
    }

    auto denied_upload = ctx.cloud_upload_file(upload_path, false, false);
    REQUIRE_FALSE(denied_upload);
    CHECK_FALSE(denied_upload.error().message.empty());
    CHECK(ctx.cloud_state().tasks.empty());
}

TEST_CASE("RuntimeContext cloud download reports missing nodes without creating tasks", "[Runtime][cloud]") {
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = test_app_dir("ubaanext-runtime-cloud-download-missing-test");

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    auto blocked = ctx.cloud_download_file("/missing.txt", options.app_data_dir / "download.bin", false);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(ctx.cloud_state().tasks.empty());
}

TEST_CASE("RuntimeContext persists connection mode config", "[Runtime]") {
    auto dir = test_app_dir("ubaanext-runtime-config-test");
    runtime::RuntimeOptions options;
    options.mock = true;
    options.app_data_dir = dir;

    auto created = runtime::RuntimeContext::create(options);
    REQUIRE(created.has_value());
    auto ctx = std::move(*created);

    auto changed = ctx.set_connection_mode("direct");
    REQUIRE(changed.has_value());

    runtime::RuntimeOptions reloaded;
    reloaded.app_data_dir = dir;
    auto second = runtime::RuntimeContext::create(reloaded);
    REQUIRE(second.has_value());
    CHECK(second->diagnostics().mode == "direct");
}
