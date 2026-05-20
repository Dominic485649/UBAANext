/**
 * @file ResultTests.cpp
 * @brief Result<T> 类型的单元测试
 *
 * 本文件对 UBAANext::Result<T> 进行全面测试，覆盖以下场景：
 *   1. 成功时包含有效值
 *   2. 成功时可隐式转换为 true（布尔上下文）
 *   3. 失败时不包含值
 *   4. 失败时携带正确的错误码与错误消息
 *   5. Result<void> 特化的成功与失败语义
 */

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Net/HttpRequest.hpp>

#include <catch2/catch_test_macros.hpp>

namespace um = UBAANext;

TEST_CASE("ErrorCode 字符串覆盖平台能力错误", "[Error]") {
    CHECK(um::error_code_to_string(um::ErrorCode::UnsupportedPlatform) == "UnsupportedPlatform");
    CHECK(um::error_code_to_string(um::ErrorCode::UnsupportedNetwork) == "UnsupportedNetwork");
    CHECK(um::error_code_to_string(um::ErrorCode::UnsupportedSecureStore) == "UnsupportedSecureStore");
    CHECK(um::error_code_to_string(um::ErrorCode::UnsupportedCrypto) == "UnsupportedCrypto");
    CHECK(um::error_code_to_string(um::ErrorCode::UnsupportedCookiePersistence) == "UnsupportedCookiePersistence");
    CHECK(um::error_code_to_string(um::ErrorCode::Timeout) == "Timeout");
    CHECK(um::error_code_to_string(um::ErrorCode::TlsError) == "TlsError");
    CHECK(um::error_code_to_string(um::ErrorCode::CryptoError) == "CryptoError");
    CHECK(um::error_code_to_string(um::ErrorCode::StorageError) == "StorageError");
}

TEST_CASE("HttpRequest 传输选项默认保持平台默认", "[http][dto]") {
    um::HttpRequest request;

    CHECK(request.transport.connect_timeout_ms == 0);
    CHECK(request.transport.request_timeout_ms == 0);
    CHECK(request.transport.proxy.empty());
    CHECK(request.transport.tls_verify_peer);
    CHECK(request.transport.tls_verify_host);
    CHECK(request.transport.redact_url_query_in_errors);
    CHECK(request.multipart_parts.empty());
    CHECK(request.redirect.follow_redirects);
}

TEST_CASE("HttpRequest 上传部件只携带已准备字节", "[http][dto]") {
    um::HttpRequest request;
    request.multipart_parts.push_back({"file", "photo.png", "image/png", {'p', 'n', 'g'}});

    REQUIRE(request.multipart_parts.size() == 1);
    CHECK(request.multipart_parts[0].field_name == "file");
    CHECK(request.multipart_parts[0].filename == "photo.png");
    CHECK(request.multipart_parts[0].content_type == "image/png");
    CHECK(request.multipart_parts[0].bytes == std::vector<unsigned char>{'p', 'n', 'g'});
}

// ============================================================
// 测试：Result 成功时包含值
// 验证当使用有效值构造 Result 时，has_value() 返回 true，
// 且解引用操作符能正确取出内部存储的值。
// ============================================================
TEST_CASE("Result 成功时包含值", "[Result]") {
    // 使用整数 42 构造一个成功的 Result
    auto result = um::Result<int>(42);

    // 断言 Result 内部包含值
    REQUIRE(result.has_value());

    // 断言解引用后得到的值等于 42
    REQUIRE(*result == 42);
}

// ============================================================
// 测试：Result 成功时可转换为 true
// 验证成功的 Result 在布尔上下文中隐式转换为 true，
// 这使得 Result 可直接用于 if 语句条件判断。
// ============================================================
TEST_CASE("Result 成功时可转换为 true", "[Result]") {
    // 构造一个值为 1 的成功 Result
    auto result = um::Result<int>(1);

    // 通过 static_cast<bool> 验证布尔转换结果为 true
    REQUIRE(static_cast<bool>(result));
}

// ============================================================
// 测试：Result 失败时不包含值
// 验证当使用错误构造 Result 时，has_value() 返回 false，
// 且布尔转换也返回 false。
// ============================================================
TEST_CASE("Result 失败时不包含值", "[Result]") {
    // 使用 Unknown 错误码和描述消息构造一个失败的 Result
    auto result = um::Result<int>(um::make_error(um::ErrorCode::Unknown, "错误"));

    // 断言 Result 不包含有效值
    REQUIRE_FALSE(result.has_value());

    // 断言布尔转换结果为 false
    REQUIRE_FALSE(static_cast<bool>(result));
}

// ============================================================
// 测试：Result 失败时携带错误信息
// 验证失败的 Result 能正确存储错误码和错误消息，
// 确保错误传播链路中信息不丢失。
// ============================================================
TEST_CASE("Result 失败时携带错误信息", "[Result]") {
    // 使用 NetworkError 错误码构造失败 Result
    auto result = um::Result<int>(um::make_error(um::ErrorCode::NetworkError, "超时"));

    // 断言错误码为 NetworkError
    REQUIRE(result.error().code == um::ErrorCode::NetworkError);

    // 断言错误消息内容为 "超时"
    REQUIRE(result.error().message == "超时");
}

// ============================================================
// 测试：Result<void> 成功
// 验证 void 特化版本在成功时的语义：
//   - has_value() 返回 true
//   - 布尔转换为 true
// 注意：void 特化不存储实际值，仅表示操作成功。
// ============================================================
TEST_CASE("Result<void> 成功", "[Result]") {
    // 默认构造一个成功的 Result<void>
    auto result = um::Result<void>{};

    // 断言成功状态
    REQUIRE(result.has_value());
    REQUIRE(static_cast<bool>(result));
}

// ============================================================
// 测试：Result<void> 失败
// 验证 void 特化版本在失败时能正确携带错误信息，
// 用于表示无返回值操作（如登出、清除缓存等）的失败。
// ============================================================
TEST_CASE("Result<void> 失败", "[Result]") {
    // 使用 AuthFailed 错误码构造失败的 Result<void>
    auto result = um::Result<void>(um::make_error(um::ErrorCode::AuthFailed, "凭据错误"));

    // 断言不包含有效值
    REQUIRE_FALSE(result.has_value());

    // 断言错误码为 AuthFailed
    REQUIRE(result.error().code == um::ErrorCode::AuthFailed);
}
