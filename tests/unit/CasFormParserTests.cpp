#include <UBAANext/Protocol/CasFormParser.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("CasFormParser extracts execution regardless of attribute order", "[protocol][cas]") {
    using UBAANext::Protocol::extract_execution;

    CHECK(extract_execution(R"HTML(<input type="hidden" name="execution" value="e1s1" />)HTML") == "e1s1");
    CHECK(extract_execution(R"HTML(<input value='e2s1' type='hidden' name='execution'>)HTML") == "e2s1");
    CHECK(extract_execution(R"HTML(<INPUT VALUE="e3s1" TYPE="hidden" NAME="execution">)HTML") == "e3s1");
    CHECK(extract_execution("<html></html>").empty());
}

TEST_CASE("CasFormParser builds login form from hidden inputs", "[protocol][cas]") {
    using namespace UBAANext::Protocol;

    const std::string html = R"HTML(
        <form>
          <input type="hidden" name="lt" value="LT-1" />
          <input type="hidden" name="execution" value="e1s1" />
          <input type="checkbox" name="rememberMe" value="true" checked />
          <input type="checkbox" name="ignored" value="false" />
          <input type="submit" name="submitButton" value="ignored" />
        </form>
    )HTML";

    auto form = build_login_form(html, "u ser", "p@ss", "e1s1");
    CHECK(form.find("lt=LT-1") != std::string::npos);
    CHECK(form.find("rememberMe=true") != std::string::npos);
    CHECK(form.find("ignored=false") == std::string::npos);
    CHECK(form.find("username=u+ser") != std::string::npos);
    CHECK(form.find("password=p%40ss") != std::string::npos);
    CHECK(form.find("submit=%E7%99%BB%E5%BD%95") != std::string::npos);
    CHECK(form.find("execution=e1s1") != std::string::npos);
    CHECK(form.find("_eventId=submit") != std::string::npos);
    CHECK(form.find("type=username_password") != std::string::npos);
}

TEST_CASE("CasFormParser treats attribute names case-insensitively", "[protocol][cas]") {
    using namespace UBAANext::Protocol;

    const std::string html = R"HTML(
        <form>
          <INPUT TYPE="hidden" NAME="lt" VALUE="LT-Upper" />
          <INPUT TYPE="hidden" NAME="execution" VALUE="e1s1" />
          <INPUT TYPE="CHECKBOX" NAME="rememberMe" VALUE="true" checked />
        </form>
    )HTML";

    auto form = build_login_form(html, "user", "pass", "e1s1", "cap value");
    CHECK(form.find("lt=LT-Upper") != std::string::npos);
    CHECK(form.find("rememberMe=true") != std::string::npos);
    CHECK(form.find("captcha=cap+value") != std::string::npos);
    CHECK(form.find("username=user") != std::string::npos);
    CHECK(form.find("password=pass") != std::string::npos);
}
