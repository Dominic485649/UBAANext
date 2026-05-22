#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>

#include <utility>

namespace UBAANext::Protocol {

const char *to_string(DownstreamSystemId system) noexcept {
    switch (system) {
    case DownstreamSystemId::Byxt: return "BYXT";
    case DownstreamSystemId::LibBook: return "LibBook";
    case DownstreamSystemId::Bykc: return "BYKC";
    case DownstreamSystemId::Cgyy: return "CGYY";
    case DownstreamSystemId::Spoc: return "SPOC";
    case DownstreamSystemId::Ygdk: return "YGDK";
    case DownstreamSystemId::Judge: return "Judge";
    case DownstreamSystemId::Signin: return "Signin";
    case DownstreamSystemId::AppBuaa: return "AppBUAA";
    case DownstreamSystemId::Score: return "Score";
    case DownstreamSystemId::Unknown: return "Unknown";
    }
    return "Unknown";
}

const char *to_string(DownstreamSessionState state) noexcept {
    switch (state) {
    case DownstreamSessionState::Ready: return "Ready";
    case DownstreamSessionState::SsoRequired: return "SsoRequired";
    case DownstreamSessionState::Expired: return "Expired";
    case DownstreamSessionState::Unavailable: return "Unavailable";
    case DownstreamSessionState::WarmupRequired: return "WarmupRequired";
    case DownstreamSessionState::UnexpectedResponse: return "UnexpectedResponse";
    case DownstreamSessionState::MissingCookie: return "MissingCookie";
    case DownstreamSessionState::MissingCasParameter: return "MissingCasParameter";
    case DownstreamSessionState::TokenMissing: return "TokenMissing";
    case DownstreamSessionState::TokenExpired: return "TokenExpired";
    case DownstreamSessionState::UnsupportedMode: return "UnsupportedMode";
    case DownstreamSessionState::ProtocolError: return "ProtocolError";
    }
    return "ProtocolError";
}

const char *to_string(DownstreamActivationStage stage) noexcept {
    switch (stage) {
    case DownstreamActivationStage::Probe: return "Probe";
    case DownstreamActivationStage::RedirectFollow: return "RedirectFollow";
    case DownstreamActivationStage::ArtifactExtract: return "ArtifactExtract";
    case DownstreamActivationStage::TokenExchange: return "TokenExchange";
    case DownstreamActivationStage::ReadyCheck: return "ReadyCheck";
    case DownstreamActivationStage::Request: return "Request";
    case DownstreamActivationStage::RetryAfterRefresh: return "RetryAfterRefresh";
    }
    return "Probe";
}

ErrorCode error_code_for_state(DownstreamSessionState state) noexcept {
    switch (state) {
    case DownstreamSessionState::Ready:
        return ErrorCode::None;
    case DownstreamSessionState::SsoRequired:
    case DownstreamSessionState::Expired:
    case DownstreamSessionState::MissingCookie:
    case DownstreamSessionState::MissingCasParameter:
    case DownstreamSessionState::TokenMissing:
    case DownstreamSessionState::TokenExpired:
        return ErrorCode::SessionExpired;
    case DownstreamSessionState::UnsupportedMode:
        return ErrorCode::InvalidArgument;
    case DownstreamSessionState::Unavailable:
        return ErrorCode::NetworkError;
    case DownstreamSessionState::WarmupRequired:
    case DownstreamSessionState::UnexpectedResponse:
    case DownstreamSessionState::ProtocolError:
        return ErrorCode::ParseError;
    }
    return ErrorCode::Unknown;
}

Error to_error(const DownstreamActivationError &error) {
    auto message = error.message.empty() ? std::string(to_string(error.state)) : error.message;
    if (!error.detail.empty()) {
        message += " [system=";
        message += to_string(error.system);
        message += " stage=";
        message += to_string(error.stage);
        message += " state=";
        message += to_string(error.state);
        message += " detail=";
        message += error.detail;
        message += "]";
    }
    return Error(error.code == ErrorCode::None ? error_code_for_state(error.state) : error.code, std::move(message));
}

DownstreamActivationError make_downstream_error(DownstreamSystemId system,
                                                DownstreamActivationStage stage,
                                                DownstreamSessionState state,
                                                std::string message,
                                                std::string detail) {
    DownstreamActivationError error;
    error.system = system;
    error.stage = stage;
    error.state = state;
    error.code = error_code_for_state(state);
    error.message = std::move(message);
    error.detail = std::move(detail);
    return error;
}

} // namespace UBAANext::Protocol
