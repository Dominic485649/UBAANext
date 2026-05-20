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

TEST_CASE("CLI main 不直接依赖具体网络实现", "[boundary][platform][cli]") {
    const auto text = read_text(std::filesystem::path(source_root()) / "apps" / "cli" / "src" / "main.cpp");
    const std::vector<std::string> forbidden = {
        "WinHttpClient.hpp",
        "DpapiSecureStore.hpp",
        "install_open_ssl_crypto_provider",
        "dynamic_cast<",
    };

    for (const auto &needle : forbidden) {
        CHECK(text.find(needle) == std::string::npos);
    }
}
