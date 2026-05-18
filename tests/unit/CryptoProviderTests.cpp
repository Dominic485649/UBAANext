#include <UBAANext/Crypto/CryptoProvider.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CryptoProvider base64 编解码", "[crypto]") {
    const std::vector<unsigned char> data = {'U', 'B', 'A', 'A'};
    auto encoded = UBAANext::base64_encode(data);
    REQUIRE(encoded == "VUJBQQ==");
    REQUIRE(UBAANext::base64_decode(encoded) == data);
}

TEST_CASE("CryptoProvider MD5 平台行为", "[crypto]") {
    auto digest = UBAANext::default_crypto_provider().md5_hex("abc");
    REQUIRE_FALSE(digest);
    REQUIRE(digest.error().code == UBAANext::ErrorCode::NotImplemented);
}

TEST_CASE("CryptoProvider SHA-1 平台行为", "[crypto]") {
    const std::vector<unsigned char> data = {'a', 'b', 'c'};
    auto digest = UBAANext::default_crypto_provider().sha1_digest(data);
    REQUIRE_FALSE(digest);
    REQUIRE(digest.error().code == UBAANext::ErrorCode::NotImplemented);
}

TEST_CASE("CryptoProvider AES CBC 平台行为", "[crypto]") {
    auto invalid = UBAANext::default_crypto_provider().aes_cbc_encrypt({0x01}, "short", "iv");
    REQUIRE_FALSE(invalid);
    REQUIRE(invalid.error().code == UBAANext::ErrorCode::InvalidArgument);

    std::vector<unsigned char> plain(16, 0x10);
    auto encrypted = UBAANext::default_crypto_provider().aes_cbc_encrypt(plain, "1234567890abcdef", "abcdef1234567890");
    REQUIRE_FALSE(encrypted);
    REQUIRE(encrypted.error().code == UBAANext::ErrorCode::NotImplemented);
}

TEST_CASE("CryptoProvider AES ECB PKCS7 平台行为", "[crypto]") {
    const std::vector<unsigned char> plain = {'U', 'B', 'A', 'A'};
    auto encrypted = UBAANext::default_crypto_provider().aes_ecb_pkcs7_encrypt(plain, "1234567890abcdef");
    REQUIRE_FALSE(encrypted);
    REQUIRE(encrypted.error().code == UBAANext::ErrorCode::NotImplemented);
}
