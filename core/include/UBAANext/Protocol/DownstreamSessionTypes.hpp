/**
 * @file DownstreamSessionTypes.hpp
 * @brief Stable downstream session state/error taxonomy.
 *
 * PartiallyMigrated: these states classify protocol activation failures; they are diagnostics, not proof of business semantic alignment.
 */
#pragma once

#include <UBAANext/Base/Error.hpp>

#include <string>

namespace UBAANext::Protocol {

enum class DownstreamSystemId {
    Byxt,
    LibBook,
    Bykc,
    Cgyy,
    Spoc,
    Ygdk,
    Judge,
    Signin,
    AppBuaa,
    Score,
    Unknown,
};

enum class DownstreamSessionState {
    Ready,
    SsoRequired,
    Expired,
    Unavailable,
    WarmupRequired,
    UnexpectedResponse,
    MissingCookie,
    MissingCasParameter,
    TokenMissing,
    TokenExpired,
    UnsupportedMode,
    ProtocolError,
};

enum class DownstreamActivationStage {
    Probe,
    RedirectFollow,
    ArtifactExtract,
    TokenExchange,
    ReadyCheck,
    Request,
    RetryAfterRefresh,
};

struct DownstreamActivationError {
    DownstreamSystemId system = DownstreamSystemId::Unknown;
    DownstreamActivationStage stage = DownstreamActivationStage::Probe;
    DownstreamSessionState state = DownstreamSessionState::ProtocolError;
    ErrorCode code = ErrorCode::Unknown;
    std::string message;
    /** Sensitive detail: may contain redacted URL/body/session context and must never hold raw credentials/cookies/tokens. */
    std::string detail;
};

[[nodiscard]] const char *to_string(DownstreamSystemId system) noexcept;
[[nodiscard]] const char *to_string(DownstreamSessionState state) noexcept;
[[nodiscard]] const char *to_string(DownstreamActivationStage stage) noexcept;
/** Stable error mapping: must not collapse downstream failures into success/empty data. */
[[nodiscard]] ErrorCode error_code_for_state(DownstreamSessionState state) noexcept;
/** Sensitive diagnostic boundary: converts downstream activation state into redaction-safe Error. */
[[nodiscard]] Error to_error(const DownstreamActivationError &error);

/** Sensitive diagnostic boundary: detail must already be redacted before storage. */
[[nodiscard]] DownstreamActivationError make_downstream_error(DownstreamSystemId system,
                                                              DownstreamActivationStage stage,
                                                              DownstreamSessionState state,
                                                              std::string message,
                                                              std::string detail = {});

} // namespace UBAANext::Protocol
