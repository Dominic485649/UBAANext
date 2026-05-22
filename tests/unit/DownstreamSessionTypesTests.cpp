#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("DownstreamSessionTypes maps auth states to structured errors", "[protocol][downstream]") {
    using namespace UBAANext::Protocol;

    CHECK(error_code_for_state(DownstreamSessionState::MissingCasParameter) == UBAANext::ErrorCode::SessionExpired);
    CHECK(error_code_for_state(DownstreamSessionState::TokenExpired) == UBAANext::ErrorCode::SessionExpired);
    CHECK(error_code_for_state(DownstreamSessionState::UnsupportedMode) == UBAANext::ErrorCode::InvalidArgument);
    CHECK(error_code_for_state(DownstreamSessionState::Unavailable) == UBAANext::ErrorCode::NetworkError);
    CHECK(error_code_for_state(DownstreamSessionState::ProtocolError) == UBAANext::ErrorCode::ParseError);

    auto activation = make_downstream_error(DownstreamSystemId::LibBook,
                                            DownstreamActivationStage::ArtifactExtract,
                                            DownstreamSessionState::MissingCasParameter,
                                            "未能获取 CAS 参数",
                                            "https://booking.lib.buaa.edu.cn/v4/login/cas?<redacted>");
    auto error = to_error(activation);
    CHECK(error.code == UBAANext::ErrorCode::SessionExpired);
    CHECK(error.message.find("LibBook") != std::string::npos);
    CHECK(error.message.find("ArtifactExtract") != std::string::npos);
    CHECK(error.message.find("MissingCasParameter") != std::string::npos);
    CHECK(error.message.find("<redacted>") != std::string::npos);
}
