#include <UBAANext/Platform/Harmony/UnsupportedSecureStore.hpp>

#include <stdexcept>

namespace UBAANext::Platform::Harmony {
namespace {

[[noreturn]] void throw_unsupported_secure_store() {
    throw std::runtime_error("Harmony secure store is not implemented; refusing plaintext fallback");
}

} // namespace

void UnsupportedSecureStore::set_string(const std::string &, const std::string &) {
    throw_unsupported_secure_store();
}

std::optional<std::string> UnsupportedSecureStore::get_string(const std::string &) const {
    return std::nullopt;
}

void UnsupportedSecureStore::remove(const std::string &) {
    throw_unsupported_secure_store();
}

void UnsupportedSecureStore::clear() {
    throw_unsupported_secure_store();
}

} // namespace UBAANext::Platform::Harmony
