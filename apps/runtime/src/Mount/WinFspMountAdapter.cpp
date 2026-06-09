#include <UBAANext/Runtime/CloudMountAdapter.hpp>

#if defined(_WIN32)
#include <UBAANext/Platform/Windows/WinFspMount.hpp>
#endif

#include <memory>
#include <utility>

namespace UBAANext::Runtime {
namespace {

#if defined(_WIN32)
class WinFspMountSession final : public ICloudMountSession {
public:
    Result<void> start(CloudVfs::CloudVfs &vfs, const CloudMountRequest &request) override {
        if (request.writable) return make_error(ErrorCode::UnsupportedPlatform, "WinFsp writable mount is not implemented yet");

        m_status.frontend = CloudMountFrontend::WinFsp;
        m_status.account_key = request.account_key;
        m_status.mount_point = request.mount_point;
        m_status.cache_dir = request.cache_dir;
        m_status.dependency_available = true;
        m_status.writable = false;

        UBAANext::Platform::Windows::WinFspMountOptions options;
        options.mount_point = request.mount_point;
        auto started = m_mount.start(vfs, std::move(options));
        if (!started) return make_error(started.error().code, started.error().message);

        m_status.running = true;
        m_status.message = "WinFsp mount started read-only";
        return {};
    }

    Result<void> stop() override {
        auto stopped = m_mount.stop();
        if (!stopped) return make_error(stopped.error().code, stopped.error().message);
        m_status.running = false;
        m_status.writable = false;
        m_status.message = "WinFsp mount stopped";
        return {};
    }

    CloudMountStatus status() const override {
        auto status = m_status;
        status.running = m_mount.running();
        return status;
    }

private:
    UBAANext::Platform::Windows::WinFspMount m_mount;
    CloudMountStatus m_status;
};
#endif

class WinFspMountAdapter final : public ICloudMountAdapter {
public:
    CloudMountFrontend frontend() const override { return CloudMountFrontend::WinFsp; }

    bool available() const override {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        return true;
#else
        return false;
#endif
    }

    Result<std::unique_ptr<ICloudMountSession>> create_session() override {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        return std::unique_ptr<ICloudMountSession>(std::make_unique<WinFspMountSession>());
#else
        return make_error(ErrorCode::UnsupportedPlatform, "WinFsp dependency is not available in this build/runtime");
#endif
    }
};

} // namespace

std::unique_ptr<ICloudMountAdapter> create_winfsp_cloud_mount_adapter() {
    return std::make_unique<WinFspMountAdapter>();
}

} // namespace UBAANext::Runtime
