#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string source_root() {
    return std::filesystem::path(UBAANEXT_SOURCE_DIR).generic_string();
}

std::string read_text(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::filesystem::path> source_files_under(const std::filesystem::path &root) {
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cxx" || ext == ".cc") {
            files.push_back(entry.path());
        }
    }
    return files;
}

} // namespace

TEST_CASE("Core 不直接包含平台实现头", "[boundary][platform]") {
    const auto root = std::filesystem::path(source_root()) / "core";
    const std::vector<std::string> forbidden = {
        "windows.h",
        "winhttp.h",
        "curl/curl.h",
        "openssl/",
        "libsecret",
        "OH_Huks",
        "OH_Crypto",
        "CryptoArchitectureKit",
        "slint",
        "Slint",
        "winfsp",
        "WinFsp",
        "cfapi",
        "CfApi",
        "cldapi",
        "Cloud Files",
        "fuse",
        "FUSE",
    };

    for (const auto &file : source_files_under(root)) {
        const auto text = read_text(file);
        INFO(file.generic_string());
        for (const auto &needle : forbidden) {
            CHECK(text.find(needle) == std::string::npos);
        }
    }
}

TEST_CASE("Core CMake 不链接平台库", "[boundary][platform]") {
    const auto text = read_text(std::filesystem::path(source_root()) / "core" / "CMakeLists.txt");
    const std::vector<std::string> forbidden = {
        "winhttp",
        "crypt32",
        "bcrypt",
        "OpenSSL::Crypto",
        "OpenSSL::SSL",
        "curl",
        "libohcrypto",
        "Slint::",
        "slint_target_sources",
        "WinFsp",
        "winfsp",
        "cfapi",
        "cldapi",
        "fuse",
        "FUSE",
    };

    for (const auto &needle : forbidden) {
        CHECK(text.find(needle) == std::string::npos);
    }
}

TEST_CASE("CLI real platform context makes plaintext credential fallback explicit", "[boundary][platform][cli]") {
    const auto text = read_text(std::filesystem::path(source_root()) / "apps" / "cli" / "src" / "PlatformContextFactory.cpp");
    const auto mock_return = text.find("return ctx;\n    }\n#endif");
    REQUIRE(mock_return != std::string::npos);
    const auto real_branch = text.substr(mock_return);

    CHECK(real_branch.find("UnsupportedHttpClient") == std::string::npos);
    CHECK(real_branch.find("CurlNetworkStack") != std::string::npos);
    CHECK(real_branch.find("LocalEncryptedFileStore") != std::string::npos);
    CHECK(real_branch.find("std::make_unique<PlainFileStore>") != std::string::npos);
    CHECK(real_branch.find("credential_persistence_secure = false") != std::string::npos);
    CHECK(real_branch.find("credential_persistence_plaintext_fallback = true") != std::string::npos);
    CHECK(real_branch.find("ctx.capabilities.secure_store = ctx.credential_persistence_secure") != std::string::npos);
}

TEST_CASE("C ABI capability struct keeps reserved bytes zeroed", "[boundary][abi]") {
    const auto header = read_text(std::filesystem::path(source_root()) / "bindings" / "c" / "include" / "UBAANext" / "Bindings" / "C" / "UbaaNative.h");
    const auto source = read_text(std::filesystem::path(source_root()) / "bindings" / "c" / "src" / "UbaaNative.cpp");

    CHECK(header.find("uint8_t reserved[14]") != std::string::npos);
    CHECK(source.find("std::memset(&out_capabilities, 0, sizeof(out_capabilities))") != std::string::npos);
}

TEST_CASE("C ABI non-JSON status returns use named constants", "[boundary][abi]") {
    const auto header = read_text(std::filesystem::path(source_root()) / "bindings" / "c" / "include" / "UBAANext" / "Bindings" / "C" / "UbaaNative.h");
    const auto source = read_text(std::filesystem::path(source_root()) / "bindings" / "c" / "src" / "UbaaNative.cpp");

    CHECK(header.find("typedef enum UbaaNextStatus") != std::string::npos);
    CHECK(header.find("UBAANEXT_STATUS_OK = 0") != std::string::npos);
    CHECK(header.find("UBAANEXT_STATUS_INVALID_ARGUMENT = -1") != std::string::npos);
    CHECK(header.find("UBAANEXT_STATUS_INVALID_CONNECTION_MODE = -2") != std::string::npos);
    CHECK(source.find("return UBAANEXT_STATUS_INVALID_ARGUMENT") != std::string::npos);
    CHECK(source.find("return UBAANEXT_STATUS_INVALID_CONNECTION_MODE") != std::string::npos);
    CHECK(source.find("return -1") == std::string::npos);
    CHECK(source.find("return -2") == std::string::npos);
}

TEST_CASE("CLI and C ABI record serializers preserve partial failure fields", "[boundary][partial]") {
    const auto cli = read_text(std::filesystem::path(source_root()) / "apps" / "cli" / "src" / "OutputFormatter.cpp");
    const auto cabi = read_text(std::filesystem::path(source_root()) / "bindings" / "c" / "src" / "UbaaNative.cpp");

    CHECK(cli.find("record_to_json") != std::string::npos);
    CHECK(cli.find("{\"status\", record.status}") != std::string::npos);
    CHECK(cli.find("{\"fields\", record.fields}") != std::string::npos);
    CHECK(cabi.find("feature_record_to_json") != std::string::npos);
    CHECK(cabi.find("{\"status\", record.status}") != std::string::npos);
    CHECK(cabi.find("{\"fields\", record.fields}") != std::string::npos);
    CHECK(cabi.find("source-error") == std::string::npos);
}

TEST_CASE("Mount and desktop capabilities are compile-time gated by platform adapters", "[boundary][platform][mount]") {
    const auto windows = read_text(std::filesystem::path(source_root()) / "platform" / "windows" / "src" / "WindowsPlatformCapabilities.cpp");
    CHECK(windows.find("caps.desktop_gui = UBAANEXT_BUILD_DESKTOP") != std::string::npos);
    CHECK(windows.find("caps.mount_windows_drive = UBAANEXT_ENABLE_WINFSP") != std::string::npos);
    CHECK(windows.find("caps.mount_windows_sync = UBAANEXT_ENABLE_CLOUD_FILES") != std::string::npos);
    CHECK(windows.find("caps.mount_linux_userspace = false") != std::string::npos);

    const auto linux = read_text(std::filesystem::path(source_root()) / "platform" / "linux" / "src" / "LinuxPlatformCapabilities.cpp");
    CHECK(linux.find("caps.desktop_gui = UBAANEXT_BUILD_DESKTOP") != std::string::npos);
    CHECK(linux.find("caps.mount_windows_drive = false") != std::string::npos);
    CHECK(linux.find("caps.mount_windows_sync = false") != std::string::npos);
    CHECK(linux.find("caps.mount_linux_userspace = UBAANEXT_ENABLE_FUSE") != std::string::npos);
}

TEST_CASE("Release install layout separates CLI desktop runtime and public headers", "[boundary][release]") {
    const auto source = std::filesystem::path(source_root());
    const auto cmake = read_text(source / "CMakeLists.txt");
    CHECK(cmake.find("install(DIRECTORY core/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})") != std::string::npos);
    CHECK(cmake.find("install(DIRECTORY apps/runtime/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})") != std::string::npos);
    CHECK(cmake.find("install(DIRECTORY apps/cli/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})") != std::string::npos);
    CHECK(cmake.find("list(APPEND UBAANEXT_INSTALL_TARGETS ubaa)") != std::string::npos);
    CHECK(cmake.find("list(APPEND UBAANEXT_INSTALL_TARGETS UBAANextDesktop)") != std::string::npos);
    CHECK(cmake.find("RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}") != std::string::npos);

    const auto cli_cmake = read_text(source / "apps" / "cli" / "CMakeLists.txt");
    CHECK(cli_cmake.find("RUNTIME_OUTPUT_DIRECTORY \"${UBAANEXT_CLI_OUTPUT_DIRECTORY}\"") != std::string::npos);
    CHECK(cli_cmake.find("OUTPUT_NAME \"ubaa\"") != std::string::npos);
    CHECK(cli_cmake.find("SUFFIX \".com\"") != std::string::npos);

    const auto desktop_cmake = read_text(source / "apps" / "desktop" / "CMakeLists.txt");
    CHECK(desktop_cmake.find("RUNTIME_OUTPUT_DIRECTORY \"${UBAANEXT_DESKTOP_OUTPUT_DIRECTORY}\"") != std::string::npos);
    CHECK(desktop_cmake.find("add_executable(UBAANextDesktop WIN32") != std::string::npos);
    CHECK(desktop_cmake.find("OUTPUT_NAME \"ubaa\"") != std::string::npos);
    CHECK(desktop_cmake.find("SUFFIX \".exe\"") != std::string::npos);
    CHECK(desktop_cmake.find("OUTPUT_NAME \"ubaa-gui\"") != std::string::npos);
    CHECK(desktop_cmake.find("Slint was requested but was not found") != std::string::npos);
}
