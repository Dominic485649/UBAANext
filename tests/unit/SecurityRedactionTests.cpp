#include "SecurityRedaction.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("CLI redaction removes sensitive headers and key values", "[security][redaction]") {
    const std::string text =
        "Cookie: SID=cookie-secret\n"
        "Set-Cookie: SESSION=set-cookie-secret\n"
        "Authorization: Bearer auth-secret\n"
        "token=token-secret&ticket=ticket-secret&cas=cas-secret&execution=exec-secret&session=session-secret&password=password-secret&captcha=captcha-secret&验证码=code-secret";

    auto redacted = UBAANextCli::redact_sensitive_text(text);

    CHECK(redacted.find("cookie-secret") == std::string::npos);
    CHECK(redacted.find("set-cookie-secret") == std::string::npos);
    CHECK(redacted.find("auth-secret") == std::string::npos);
    CHECK(redacted.find("token-secret") == std::string::npos);
    CHECK(redacted.find("ticket-secret") == std::string::npos);
    CHECK(redacted.find("cas-secret") == std::string::npos);
    CHECK(redacted.find("exec-secret") == std::string::npos);
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

TEST_CASE("CLI redaction removes diagnostics URL paths HTML and business secrets", "[security][redaction]") {
    const std::string text =
        "GET https://example.edu/api/orders?ticket=ticket-secret&grade=99&lock_code=LOCK-123#frag\n"
        "Location: https://example.edu/callback?token=token-secret&session=session-secret\n"
        "path=C:/secret/grade.html filename=score.csv file=/data/local/tmp/upload.jpg\n"
        "bookingId=seat-booking-1 place=操场 location=北区 lockCode=LOCK-456\n"
        "<html><body>raw score=99 token=html-secret</body></html>";

    auto redacted = UBAANextCli::redact_sensitive_text(text);

    CHECK(redacted.find("ticket-secret") == std::string::npos);
    CHECK(redacted.find("grade=99") == std::string::npos);
    CHECK(redacted.find("LOCK-123") == std::string::npos);
    CHECK(redacted.find("token-secret") == std::string::npos);
    CHECK(redacted.find("session-secret") == std::string::npos);
    CHECK(redacted.find("C:/secret/grade.html") == std::string::npos);
    CHECK(redacted.find("score.csv") == std::string::npos);
    CHECK(redacted.find("/data/local/tmp/upload.jpg") == std::string::npos);
    CHECK(redacted.find("seat-booking-1") == std::string::npos);
    CHECK(redacted.find("操场") == std::string::npos);
    CHECK(redacted.find("北区") == std::string::npos);
    CHECK(redacted.find("LOCK-456") == std::string::npos);
    CHECK(redacted.find("raw score=99") == std::string::npos);
    CHECK(redacted.find("html-secret") == std::string::npos);
    CHECK(redacted.find("[REDACTED]") != std::string::npos);
}
