#include "PlatformContextFactory.hpp"

#include "PlainFileStore.hpp"

#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#if defined(_WIN32)
#include <UBAANext/Platform/Windows/DpapiSecureStore.hpp>
#include <UBAANext/Platform/Windows/WinHttpClient.hpp>
#include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>
#endif

#if UBAANEXT_ENABLE_MOCKS
#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>
#endif

#include <utility>

namespace UBAANextCli {
namespace {

class NoopCookieStore final : public UBAANext::ICookieStore {
public:
    UBAANext::Result<UBAANext::CookieJar> load() override {
        return UBAANext::CookieJar{};
    }

    UBAANext::Result<void> save(const UBAANext::CookieJar &cookies) override {
        (void)cookies;
        return {};
    }

    UBAANext::Result<void> clear() override {
        return {};
    }
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

#if !defined(_WIN32)
class UnsupportedHttpClient final : public UBAANext::IHttpClient {
public:
    [[nodiscard]] UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &request) override {
        (void)request;
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedNetwork, "当前平台尚未接入真实网络能力");
    }
};
#endif

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
    UBAANext::WinHttpConfig cfg;
    cfg.follow_redirects = false;
    if (!options.config.proxy.empty()) {
        cfg.proxy = options.config.proxy;
    }
    auto client = std::make_unique<UBAANext::WinHttpClient>(cfg);
    client->load_cookies(options.cookie_file_path.string());
    auto *win_http = client.get();
    ctx.http = std::move(client);
    ctx.save_cookies = [win_http, path = options.cookie_file_path.string()] {
        win_http->save_cookies(path);
    };
    ctx.clear_cookies = [win_http] {
        win_http->cookies().clear();
    };
    ctx.capabilities = UBAANext::Platform::Windows::WindowsPlatformCapabilities{}.capabilities();
    ctx.network_stack = std::make_unique<HttpClientNetworkStack>(*ctx.http);
    ctx.store = std::make_unique<UBAANext::Platform::Windows::DpapiSecureStore>(options.session_file_path);
#else
    ctx.http = std::make_unique<UnsupportedHttpClient>();
    ctx.capabilities.real_network = false;
    ctx.capabilities.secure_store = false;
    ctx.capabilities.secure_cookie_persistence = false;
    ctx.capabilities.cookie_persistence = false;
    ctx.capabilities.redirect_control = false;
    ctx.capabilities.openssl_crypto = true;
    ctx.capabilities.app_data_path = true;
    ctx.capabilities.upload_bytes = true;
    ctx.capabilities.live_login = false;
    ctx.capabilities.write_operations = false;
    ctx.network_stack = std::make_unique<HttpClientNetworkStack>(*ctx.http);
    ctx.store = std::make_unique<PlainFileStore>(options.session_file_path);
#endif
    ctx.cache = std::make_unique<UBAANext::MemoryCacheStore>();
    return ctx;
}

void save_platform_cookies(AppContext &ctx) {
    if (ctx.network_stack) {
        auto loaded = ctx.network_stack->cookie_store().load();
        if (loaded) {
            auto saved = ctx.network_stack->cookie_store().save(*loaded);
            if (saved) {
                return;
            }
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
