#include "SecurityRedaction.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("CLI redaction removes sensitive headers and key values", "[security][redaction]") {
    const std::string text =
        "Cookie: SID=cookie-secret\n"
        "Set-Cookie: SESSION=set-cookie-secret\n"
        "Authorization: Bearer auth-secret\n"
        "token=token-secret&ticket=ticket-secret&session=session-secret&password=password-secret&captcha=captcha-secret&验证码=code-secret";

    auto redacted = UBAANextCli::redact_sensitive_text(text);

    CHECK(redacted.find("cookie-secret") == std::string::npos);
    CHECK(redacted.find("set-cookie-secret") == std::string::npos);
    CHECK(redacted.find("auth-secret") == std::string::npos);
    CHECK(redacted.find("token-secret") == std::string::npos);
    CHECK(redacted.find("ticket-secret") == std::string::npos);
    CHECK(redacted.find("session-secret") == std::string::npos);
    CHECK(redacted.find("password-secret") == std::string::npos);
    CHECK(redacted.find("captcha-secret") == std::string::npos);
    CHECK(redacted.find("code-secret") == std::string::npos);
}

TEST_CASE("CLI redaction keeps non-sensitive photo field", "[security][redaction]") {
    auto redacted = UBAANextCli::redact_sensitive_text("photo=avatar.png&photo_path=C:/secret/avatar.png");

    CHECK(redacted.find("photo=avatar.png") != std::string::npos);
    CHECK(redacted.find("C:/secret/avatar.png") == std::string::npos);
}
