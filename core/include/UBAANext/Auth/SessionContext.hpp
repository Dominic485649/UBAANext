#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Account.hpp>

namespace UBAANext::Auth {

struct SessionContext {
    /** Sensitive output: restored account fields must remain redaction-aware. */
    Model::Account account;
    /** Sensitive session boundary: restored routing mode does not prove live platform reachability. */
    ConnectionMode connection_mode = ConnectionMode::WebVPN;
};

/** Sensitive output: restores local session context or returns stable storage/session errors. */
[[nodiscard]] Result<SessionContext> restore_session_context(AuthService &auth_service);

} // namespace UBAANext::Auth
