#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CryptoProvider base64 编码", "[crypto]") {
    const std::vector<unsigned char> data = {'U', 'B', 'A', 'A'};
    REQUIRE(UBAANext::base64_encode(data) == "VUJBQQ==");
}

TEST_CASE("CryptoProvider MD5 平台行为", "[crypto]") {
    auto digest = UBAANext::default_crypto_provider().md5_hex("abc");
#ifdef _WIN32
    REQUIRE(digest);
    REQUIRE(*digest == "900150983cd24fb0d6963f7d28e17f72");
#else
    REQUIRE_FALSE(digest);
    REQUIRE(digest.error().code == UBAANext::ErrorCode::NotImplemented);
#endif
}

TEST_CASE("CryptoProvider AES CBC 平台行为", "[crypto]") {
    auto invalid = UBAANext::default_crypto_provider().aes_cbc_encrypt({0x01}, "short", "iv");
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().code == UBAANext::ErrorCode::InvalidArgument);

    std::vector<unsigned char> plain(16, 0x10);
    auto encrypted = UBAANext::default_crypto_provider().aes_cbc_encrypt(plain, "1234567890abcdef", "abcdef1234567890");
#ifdef _WIN32
    REQUIRE(encrypted);
    REQUIRE(encrypted->size() == 16);
#else
    REQUIRE_FALSE(encrypted);
    REQUIRE(encrypted.error().code == UBAANext::ErrorCode::NotImplemented);
#endif
}
