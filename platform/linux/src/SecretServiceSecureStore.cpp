#include <UBAANext/Platform/Linux/SecretServiceSecureStore.hpp>

#include <UBAANext/Base/Result.hpp>

#if UBAANEXT_ENABLE_LINUX_LIBSECRET
#include <libsecret/secret.h>
#endif

#include <stdexcept>
#include <utility>

namespace UBAANext::Platform::Linux {
namespace {

constexpr const char *kSchemaName = "dev.ubaanext.secure-store";
constexpr const char *kServiceAttr = "ubaanext";
constexpr const char *kKeyAttr = "key";

#if UBAANEXT_ENABLE_LINUX_LIBSECRET
const SecretSchema *secure_store_schema() {
    static const SecretSchema schema = {
        kSchemaName,
        SECRET_SCHEMA_NONE,
        {
            {kServiceAttr, SECRET_SCHEMA_ATTRIBUTE_STRING},
            {kKeyAttr, SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, static_cast<SecretSchemaAttributeType>(0)},
        },
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };
    return &schema;
}
#endif

[[noreturn]] void throw_unsupported() {
    throw std::runtime_error("Linux Secret Service secure store is not available; refusing plaintext fallback");
}

} // namespace

SecretServiceSecureStore::SecretServiceSecureStore(std::string collection)
    : m_collection(std::move(collection)) {}

void SecretServiceSecureStore::set_string(const std::string &key, const std::string &value) {
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
    GError *error = nullptr;
    const auto label = std::string("UBAANext ") + key;
    if (!secret_password_store_sync(secure_store_schema(), SECRET_COLLECTION_DEFAULT, label.c_str(), value.c_str(), nullptr, &error,
                                    kServiceAttr, m_collection.c_str(), kKeyAttr, key.c_str(), nullptr)) {
        std::string message = error && error->message ? error->message : "unknown error";
        if (error) g_error_free(error);
        throw std::runtime_error("Linux Secret Service 写入失败: " + message);
    }
#else
    (void)key;
    (void)value;
    throw_unsupported();
#endif
}

std::optional<std::string> SecretServiceSecureStore::get_string(const std::string &key) const {
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
    GError *error = nullptr;
    char *value = secret_password_lookup_sync(secure_store_schema(), nullptr, &error,
                                              kServiceAttr, m_collection.c_str(), kKeyAttr, key.c_str(), nullptr);
    if (error) {
        std::string message = error->message ? error->message : "unknown error";
        g_error_free(error);
        throw std::runtime_error("Linux Secret Service 读取失败: " + message);
    }
    if (!value) {
        return std::nullopt;
    }
    std::string result(value);
    secret_password_free(value);
    return result;
#else
    (void)key;
    return std::nullopt;
#endif
}

void SecretServiceSecureStore::remove(const std::string &key) {
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
    GError *error = nullptr;
    if (!secret_password_clear_sync(secure_store_schema(), nullptr, &error,
                                    kServiceAttr, m_collection.c_str(), kKeyAttr, key.c_str(), nullptr)) {
        std::string message = error && error->message ? error->message : "unknown error";
        if (error) g_error_free(error);
        throw std::runtime_error("Linux Secret Service 删除失败: " + message);
    }
#else
    (void)key;
    throw_unsupported();
#endif
}

void SecretServiceSecureStore::clear() {
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
    GError *error = nullptr;
    if (!secret_password_clear_sync(secure_store_schema(), nullptr, &error,
                                    kServiceAttr, m_collection.c_str(), nullptr)) {
        std::string message = error && error->message ? error->message : "unknown error";
        if (error) g_error_free(error);
        throw std::runtime_error("Linux Secret Service 清理失败: " + message);
    }
#else
    throw_unsupported();
#endif
}

} // namespace UBAANext::Platform::Linux
