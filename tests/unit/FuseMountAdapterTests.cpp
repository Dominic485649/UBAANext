#include <UBAANext/Runtime/CloudMountManager.hpp>
#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace runtime = UBAANext::Runtime;

namespace {

class MountManagerHttpClient final : public UBAANext::IHttpClient {
public:
    UBAANext::Result<UBAANext::HttpResponse> send(const UBAANext::HttpRequest &) override {
        return UBAANext::make_error(UBAANext::ErrorCode::NetworkError, "unexpected request");
    }
};

class MountManagerCookieStore final : public UBAANext::ICookieStore {
public:
    UBAANext::Result<UBAANext::CookieJar> load() override { return jar; }
    UBAANext::Result<void> save(const UBAANext::CookieJar &cookies) override {
        jar = cookies;
        return {};
    }
    UBAANext::Result<void> save_current() override { return {}; }
    UBAANext::Result<void> clear() override {
        jar.clear();
        return {};
    }
    const UBAANext::CookieJar *current() const override { return &jar; }

    UBAANext::CookieJar jar;
};

struct FakeMountCounters {
    int create_session_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    std::vector<runtime::CloudMountRequest> start_requests;
};

class FakeMountSession final : public runtime::ICloudMountSession {
public:
    explicit FakeMountSession(FakeMountCounters &counters, runtime::CloudMountFrontend frontend)
        : m_counters(counters), m_frontend(frontend) {}

    UBAANext::Result<void> start(UBAANext::CloudVfs::CloudVfs &, const runtime::CloudMountRequest &request) override {
        ++m_counters.start_calls;
        m_counters.start_requests.push_back(request);
        m_status.frontend = m_frontend;
        m_status.account_key = request.account_key;
        m_status.mount_point = request.mount_point;
        m_status.cache_dir = request.cache_dir;
        m_status.running = true;
        m_status.writable = request.writable;
        m_status.dependency_available = true;
        m_status.message = "fake mount started";
        return {};
    }

    UBAANext::Result<void> stop() override {
        ++m_counters.stop_calls;
        m_status.running = false;
        m_status.writable = false;
        m_status.message = "fake mount stopped";
        return {};
    }

    runtime::CloudMountStatus status() const override { return m_status; }

private:
    FakeMountCounters &m_counters;
    runtime::CloudMountFrontend m_frontend;
    runtime::CloudMountStatus m_status;
};

class FakeMountAdapter final : public runtime::ICloudMountAdapter {
public:
    FakeMountAdapter(FakeMountCounters &counters, runtime::CloudMountFrontend frontend)
        : m_counters(counters), m_frontend(frontend) {}

    runtime::CloudMountFrontend frontend() const override { return m_frontend; }
    bool available() const override { return true; }
    UBAANext::Result<std::unique_ptr<runtime::ICloudMountSession>> create_session() override {
        ++m_counters.create_session_calls;
        return std::unique_ptr<runtime::ICloudMountSession>(std::make_unique<FakeMountSession>(m_counters, m_frontend));
    }

private:
    FakeMountCounters &m_counters;
    runtime::CloudMountFrontend m_frontend;
};

struct MountManagerFixture {
    MountManagerHttpClient http;
    MountManagerCookieStore cookies;
    UBAANext::MemoryCacheStore cache;
    UBAANext::CloudService service{http, &cookies, cache, UBAANext::ConnectionMode::Direct};
    UBAANext::CloudVfs::MemoryCloudVfsContentCache content_cache;
    UBAANext::CloudVfs::CloudVfs vfs{service, content_cache};
};

} // namespace

TEST_CASE("CloudMountManager starts and stops adapter sessions", "[runtime][mount]") {
    MountManagerFixture fixture;
    FakeMountCounters counters;
    runtime::CloudMountManager mounts(&fixture.vfs);
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(counters, runtime::CloudMountFrontend::CloudFiles));

    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::CloudFiles;
    request.account_key = "account-a";
    request.mount_point = "B:";
    request.cache_dir = "cache-a";
    request.writable = true;

    auto started = mounts.start(request);
    REQUIRE(started);
    CHECK(counters.create_session_calls == 1);
    CHECK(counters.start_calls == 1);
    CHECK(counters.start_requests.front().writable);
    CHECK(started->running);
    CHECK(started->writable);

    auto statuses = mounts.statuses();
    REQUIRE(statuses.size() == 1);
    CHECK(statuses.front().running);
    CHECK(statuses.front().dependency_available);

    auto stopped = mounts.stop(runtime::CloudMountFrontend::CloudFiles);
    REQUIRE(stopped);
    CHECK(counters.stop_calls == 1);
    CHECK_FALSE(stopped->running);
    CHECK_FALSE(stopped->writable);

    statuses = mounts.statuses();
    REQUIRE(statuses.size() == 1);
    CHECK_FALSE(statuses.front().running);
    CHECK(statuses.front().dependency_available);
}

TEST_CASE("CloudMountManager stops active sessions when destroyed", "[runtime][mount]") {
    MountManagerFixture fixture;
    FakeMountCounters counters;

    {
        runtime::CloudMountManager mounts(&fixture.vfs);
        mounts.register_adapter(std::make_unique<FakeMountAdapter>(counters, runtime::CloudMountFrontend::CloudFiles));

        runtime::CloudMountRequest request;
        request.frontend = runtime::CloudMountFrontend::CloudFiles;
        request.account_key = "account-a";
        request.mount_point = "B:";
        request.cache_dir = "cache-a";

        REQUIRE(mounts.start(request));
        CHECK(counters.stop_calls == 0);
    }

    CHECK(counters.stop_calls == 1);
}

TEST_CASE("CloudMountManager stops existing session before replacing adapter", "[runtime][mount]") {
    MountManagerFixture fixture;
    FakeMountCounters first_counters;
    FakeMountCounters second_counters;
    runtime::CloudMountManager mounts(&fixture.vfs);
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(first_counters, runtime::CloudMountFrontend::CloudFiles));

    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::CloudFiles;
    request.account_key = "account-a";
    request.mount_point = "B:";
    request.cache_dir = "cache-a";

    REQUIRE(mounts.start(request));
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(second_counters, runtime::CloudMountFrontend::CloudFiles));

    CHECK(first_counters.stop_calls == 1);
    CHECK(second_counters.create_session_calls == 0);
    CHECK_FALSE(mounts.statuses().front().running);
}

TEST_CASE("CloudMountManager stops active sessions when VFS changes", "[runtime][mount]") {
    MountManagerFixture first_fixture;
    MountManagerFixture second_fixture;
    FakeMountCounters counters;
    runtime::CloudMountManager mounts(&first_fixture.vfs);
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(counters, runtime::CloudMountFrontend::CloudFiles));

    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::CloudFiles;
    request.account_key = "account-a";
    request.mount_point = "B:";
    request.cache_dir = "cache-a";

    REQUIRE(mounts.start(request));
    mounts.set_vfs(second_fixture.vfs);

    CHECK(counters.stop_calls == 1);
    CHECK_FALSE(mounts.statuses().front().running);
}

TEST_CASE("CloudMountManager fails closed for duplicate writable frontends", "[runtime][mount]") {
    MountManagerFixture fixture;
    FakeMountCounters cloud_files_counters;
    FakeMountCounters fuse_counters;
    runtime::CloudMountManager mounts(&fixture.vfs);
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(cloud_files_counters, runtime::CloudMountFrontend::CloudFiles));
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(fuse_counters, runtime::CloudMountFrontend::Fuse));

    runtime::CloudMountRequest cloud_files_request;
    cloud_files_request.frontend = runtime::CloudMountFrontend::CloudFiles;
    cloud_files_request.account_key = "account-a";
    cloud_files_request.mount_point = "B:";
    cloud_files_request.cache_dir = "same-cache";
    cloud_files_request.writable = true;
    REQUIRE(mounts.start(cloud_files_request));

    runtime::CloudMountRequest fuse_request = cloud_files_request;
    fuse_request.frontend = runtime::CloudMountFrontend::Fuse;
    fuse_request.mount_point = "/tmp/ubaanext-fuse";

    auto rejected = mounts.start(fuse_request);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().code == UBAANext::ErrorCode::InvalidArgument);
    CHECK(fuse_counters.create_session_calls == 0);
    CHECK(fuse_counters.start_calls == 0);

    auto statuses = mounts.statuses();
    REQUIRE(statuses.size() == 2);
    CHECK(mounts.has_writable_frontend("account-a", "same-cache"));
}

TEST_CASE("CloudMountManager serializes concurrent writable frontend starts", "[runtime][mount]") {
    MountManagerFixture fixture;
    FakeMountCounters cloud_files_counters;
    FakeMountCounters fuse_counters;
    runtime::CloudMountManager mounts(&fixture.vfs);
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(cloud_files_counters, runtime::CloudMountFrontend::CloudFiles));
    mounts.register_adapter(std::make_unique<FakeMountAdapter>(fuse_counters, runtime::CloudMountFrontend::Fuse));

    runtime::CloudMountRequest cloud_files_request;
    cloud_files_request.frontend = runtime::CloudMountFrontend::CloudFiles;
    cloud_files_request.account_key = "account-a";
    cloud_files_request.mount_point = "B:";
    cloud_files_request.cache_dir = "same-cache";
    cloud_files_request.writable = true;

    runtime::CloudMountRequest fuse_request = cloud_files_request;
    fuse_request.frontend = runtime::CloudMountFrontend::Fuse;
    fuse_request.mount_point = "/tmp/ubaanext-fuse";

    std::atomic<int> ready{0};
    std::atomic<bool> release{false};
    std::atomic<int> successful_starts{0};
    std::atomic<int> rejected_starts{0};

    auto start_mount = [&](runtime::CloudMountRequest request) {
        ++ready;
        while (!release.load()) {
            std::this_thread::yield();
        }
        auto started = mounts.start(request);
        if (started) {
            ++successful_starts;
        } else if (started.error().code == UBAANext::ErrorCode::InvalidArgument) {
            ++rejected_starts;
        }
    };

    std::thread cloud_files_thread(start_mount, cloud_files_request);
    std::thread fuse_thread(start_mount, fuse_request);
    while (ready.load() != 2) {
        std::this_thread::yield();
    }
    release.store(true);
    cloud_files_thread.join();
    fuse_thread.join();

    CHECK(successful_starts == 1);
    CHECK(rejected_starts == 1);
    CHECK(mounts.has_writable_frontend("account-a", "same-cache"));

    const auto statuses = mounts.statuses();
    const auto running_writable = std::count_if(statuses.begin(), statuses.end(), [](const runtime::CloudMountStatus &status) {
        return status.running && status.writable;
    });
    CHECK(running_writable == 1);
}

TEST_CASE("Cloud Files mount frontend reports unavailable when dependency is absent", "[runtime][mount][cloud-files]") {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_CLOUD_FILES) && UBAANEXT_ENABLE_CLOUD_FILES
    SKIP("Cloud Files dependency is available in this build");
#else
    runtime::CloudMountManager mounts;
    mounts.register_adapter(runtime::create_cloud_files_mount_adapter());
    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::CloudFiles;
    request.account_key = "test";
    request.mount_point = "B:";

    auto started = mounts.start(request);
    REQUIRE_FALSE(started);
    CHECK(started.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    REQUIRE(mounts.statuses().size() == 1);
    const auto status = mounts.statuses().front();
    CHECK(status.frontend == runtime::CloudMountFrontend::CloudFiles);
    CHECK_FALSE(status.running);
    CHECK_FALSE(status.writable);
    CHECK_FALSE(status.dependency_available);
    CHECK(status.message.find("dependency is not available") != std::string::npos);
#endif
}

TEST_CASE("FUSE mount frontend reports unavailable when dependency is absent", "[runtime][mount][fuse]") {
#if defined(__linux__) && defined(UBAANEXT_ENABLE_FUSE) && UBAANEXT_ENABLE_FUSE
    SKIP("FUSE dependency is available in this build");
#else
    runtime::CloudMountManager mounts;
    mounts.register_adapter(runtime::create_fuse_cloud_mount_adapter());

    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::Fuse;
    request.account_key = "test";
    request.mount_point = "/tmp/ubaanext-fuse-test";

    auto started = mounts.start(request);
    REQUIRE_FALSE(started);
    CHECK(started.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    CHECK_FALSE(runtime::CloudMountManager::dependency_available(runtime::CloudMountFrontend::Fuse));
#endif
}

TEST_CASE("WinFsp mount frontend reports unavailable when dependency is absent", "[runtime][mount][winfsp]") {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
    SKIP("WinFsp dependency is available in this build");
#else
    runtime::CloudMountManager mounts;
    mounts.register_adapter(runtime::create_winfsp_cloud_mount_adapter());

    runtime::CloudMountRequest request;
    request.frontend = runtime::CloudMountFrontend::WinFsp;
    request.account_key = "test";
    request.mount_point = "B:";

    auto started = mounts.start(request);
    REQUIRE_FALSE(started);
    CHECK(started.error().code == UBAANext::ErrorCode::UnsupportedPlatform);
    CHECK_FALSE(runtime::CloudMountManager::dependency_available(runtime::CloudMountFrontend::WinFsp));
    REQUIRE(mounts.statuses().size() == 1);
    CHECK_FALSE(mounts.statuses().front().dependency_available);
#endif
}
