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
    };

    for (const auto &needle : forbidden) {
        CHECK(text.find(needle) == std::string::npos);
    }
}

TEST_CASE("CLI real platform context does not use plaintext session store", "[boundary][platform][cli]") {
    const auto text = read_text(std::filesystem::path(source_root()) / "apps" / "cli" / "src" / "PlatformContextFactory.cpp");
    const auto mock_return = text.find("return ctx;\n    }\n#endif");
    REQUIRE(mock_return != std::string::npos);
    const auto real_branch = text.substr(mock_return);

    CHECK(real_branch.find("std::make_unique<PlainFileStore>") == std::string::npos);
    CHECK(real_branch.find("UnsupportedHttpClient") == std::string::npos);
    CHECK(real_branch.find("CurlNetworkStack") != std::string::npos);
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
