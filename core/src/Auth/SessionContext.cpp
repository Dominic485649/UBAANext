#include <UBAANext/Auth/SessionContext.hpp>

namespace UBAANext::Auth {

Result<SessionContext> restore_session_context(AuthService &auth_service) {
    auto account = auth_service.restore_session();
    if (!account) {
        return make_error(account.error().code, account.error().message);
    }

    SessionContext context;
    context.account = std::move(*account);
    context.connection_mode = auth_service.connection_mode();
    return context;
}

} // namespace UBAANext::Auth
