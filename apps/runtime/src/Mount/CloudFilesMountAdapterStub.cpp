#include <UBAANext/Runtime/CloudMountAdapter.hpp>

#include <memory>

namespace UBAANext::Runtime {
namespace {

class CloudFilesMountAdapterStub final : public ICloudMountAdapter {
public:
    [[nodiscard]] CloudMountFrontend frontend() const override {
        return CloudMountFrontend::CloudFiles;
    }

    [[nodiscard]] bool available() const override {
        return false;
    }

    [[nodiscard]] Result<std::unique_ptr<ICloudMountSession>> create_session() override {
        return make_error(ErrorCode::UnsupportedPlatform, "Cloud Files dependency is not available in this build/runtime");
    }
};

} // namespace

std::unique_ptr<ICloudMountAdapter> create_cloud_files_mount_adapter() {
    return std::make_unique<CloudFilesMountAdapterStub>();
}

} // namespace UBAANext::Runtime
