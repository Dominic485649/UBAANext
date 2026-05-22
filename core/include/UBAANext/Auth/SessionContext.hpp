#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Account.hpp>

namespace UBAANext::Auth {

struct SessionContext {
    Model::Account account;
    ConnectionMode connection_mode = ConnectionMode::WebVPN;
};

[[nodiscard]] Result<SessionContext> restore_session_context(AuthService &auth_service);

} // namespace UBAANext::Auth
