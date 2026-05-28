#include "PlatformContextFactory.hpp"

#include "PlainFileStore.hpp"

#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>
#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#ifndef UBAANEXT_ENABLE_LINUX_LIBSECRET
#define UBAANEXT_ENABLE_LINUX_LIBSECRET 0
#endif

#if defined(_WIN32)
#include <UBAANext/Platform/Windows/DpapiSecureStore.hpp>
#include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>
#elif defined(__linux__)
#include <UBAANext/Platform/Linux/LinuxPlatformCapabilities.hpp>
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
#include <UBAANext/Platform/Linux/SecretServiceSecureStore.hpp>
#endif
#elif defined(__OHOS__)
#include <UBAANext/Platform/Harmony/HarmonyPlatformCapabilities.hpp>
#endif

#if UBAANEXT_ENABLE_MOCKS
#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>
#endif

#include <optional>
#include <unordered_map>
#include <utility>

namespace UBAANextCli {
namespace {

class NoopCookieStore final : public UBAANext::ICookieStore {
public:
    /** Unsupported cookie persistence: used only for mock/client wrappers and never as a real session store. */
    UBAANext::Result<UBAANext::CookieJar> load() override {
        return UBAANext::CookieJar{};
    }

    UBAANext::Result<void> save(const UBAANext::CookieJar &cookies) override {
        (void)cookies;
        return {};
    }

    UBAANext::Result<void> save_current() override {
        return {};
    }

    UBAANext::Result<void> clear() override {
        return {};
    }
};

class VolatileSecureStore final : public UBAANext::ISecureStore {
public:
    /** Fallback volatile storage: does not prove secure_store or cookie/session persistence capability. */
    void set_string(const std::string &key, const std::string &value) override {
        m_values[key] = value;
    }

    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override {
        const auto it = m_values.find(key);
        if (it == m_values.end()) return std::nullopt;
        return it->second;
    }

    void remove(const std::string &key) override {
        m_values.erase(key);
    }

    void clear() override {
        m_values.clear();
    }

private:
    std::unordered_map<std::string, std::string> m_values;
};

class StaticRedirectController final : public UBAANext::IRedirectController {
public:
    explicit StaticRedirectController(UBAANext::RedirectOptions defaults = {})
        : m_defaults(defaults) {}

    [[nodiscard]] UBAANext::RedirectOptions defaults() const override {
        return m_defaults;
    }

private:
    UBAANext::RedirectOptions m_defaults;
};

class HttpClientNetworkStack final : public UBAANext::INetworkStack {
public:
    explicit HttpClientNetworkStack(UBAANext::IHttpClient &http_client)
        : m_http_client(http_client) {}

    [[nodiscard]] UBAANext::IHttpClient &http_client() override {
        return m_http_client;
    }

    [[nodiscard]] UBAANext::ICookieStore &cookie_store() override {
        return m_cookie_store;
    }

    [[nodiscard]] UBAANext::IRedirectController &redirect_controller() override {
        return m_redirect_controller;
    }

private:
    UBAANext::IHttpClient &m_http_client;
    NoopCookieStore m_cookie_store;
    StaticRedirectController m_redirect_controller;
};

} // namespace

AppContext create_current_platform_context(const PlatformContextOptions &options) {
    UBAANext::Platform::OpenSSL::install_open_ssl_crypto_provider();

    AppContext ctx;
    ctx.mock_mode = options.mock;
#if UBAANEXT_ENABLE_MOCKS
    if (options.mock) {
        ctx.conn_mode = UBAANext::ConnectionMode::Mock;
    } else
#endif
    {
        ctx.conn_mode = (options.mode == "direct") ? UBAANext::ConnectionMode::Direct : UBAANext::ConnectionMode::WebVPN;
    }
    ctx.config = options.config;
    ctx.cookie_file_path = options.cookie_file_path;
    ctx.crypto = &UBAANext::default_crypto_provider();
    ctx.cache = std::make_unique<UBAANext::MemoryCacheStore>();

#if UBAANEXT_ENABLE_MOCKS
    if (options.mock) {
        ctx.http = std::make_unique<UBAANextMocks::MockHttpClient>();
        ctx.network_stack = std::make_unique<HttpClientNetworkStack>(*ctx.http);
        ctx.cache = std::make_unique<UBAANextMocks::MockCacheStore>();
#if defined(_WIN32)
        ctx.store = std::make_unique<UBAANext::Platform::Windows::DpapiSecureStore>(options.session_file_path);
#else
        ctx.store = std::make_unique<PlainFileStore>(options.session_file_path);
#endif
        return ctx;
    }
#endif

#if defined(_WIN32)
    ctx.store = std::make_unique<UBAANext::Platform::Windows::DpapiSecureStore>(options.session_file_path);
    ctx.network_stack = std::make_unique<UBAANext::Platform::Curl::CurlNetworkStack>(*ctx.store);
    auto loaded_cookies = ctx.network_stack->cookie_store().load();
    (void)loaded_cookies;
    ctx.capabilities = UBAANext::Platform::Windows::WindowsPlatformCapabilities{}.capabilities();
#elif defined(__linux__)
    ctx.capabilities = UBAANext::Platform::Linux::LinuxPlatformCapabilities{}.capabilities();
#if UBAANEXT_ENABLE_LINUX_LIBSECRET
    ctx.store = std::make_unique<UBAANext::Platform::Linux::SecretServiceSecureStore>();
    ctx.network_stack = std::make_unique<UBAANext::Platform::Curl::CurlNetworkStack>(*ctx.store);
    auto loaded_cookies = ctx.network_stack->cookie_store().load();
    (void)loaded_cookies;
#else
    ctx.store = std::make_unique<VolatileSecureStore>();
    ctx.network_stack = std::make_unique<UBAANext::Platform::Curl::CurlNetworkStack>();
#endif
#elif defined(__OHOS__)
    ctx.capabilities = UBAANext::Platform::Harmony::HarmonyPlatformCapabilities{}.capabilities();
    ctx.store = std::make_unique<VolatileSecureStore>();
    ctx.network_stack = std::make_unique<UBAANext::Platform::Curl::CurlNetworkStack>();
#else
    ctx.store = std::make_unique<VolatileSecureStore>();
    ctx.network_stack = std::make_unique<UBAANext::Platform::Curl::CurlNetworkStack>();
    ctx.capabilities.real_network = true;
    ctx.capabilities.secure_store = false;
    ctx.capabilities.secure_cookie_persistence = false;
    ctx.capabilities.cookie_persistence = false;
    ctx.capabilities.redirect_control = true;
    ctx.capabilities.openssl_crypto = true;
    ctx.capabilities.app_data_path = true;
    ctx.capabilities.upload_bytes = true;
    ctx.capabilities.live_login = false;
    // WriteGated fail-closed default: real remote mutations require an explicit platform opt-in.
    ctx.capabilities.write_operations = false;
#endif
    return ctx;
}

void save_platform_cookies(AppContext &ctx) {
    if (ctx.network_stack) {
        auto saved = ctx.network_stack->cookie_store().save_current();
        if (saved) {
            return;
        }
    }
    if (ctx.save_cookies) {
        ctx.save_cookies();
    }
}

void clear_platform_cookies(AppContext &ctx) {
    if (ctx.network_stack) {
        auto cleared = ctx.network_stack->cookie_store().clear();
        if (cleared) {
            return;
        }
    }
    if (ctx.clear_cookies) {
        ctx.clear_cookies();
    }
}

} // namespace UBAANextCli
