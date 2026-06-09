#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/CloudVfs/CloudVfs.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace UBAANext::Runtime {

enum class CloudMountFrontend {
    WinFsp,
    CloudFiles,
    Fuse,
};

struct CloudMountRequest {
    CloudMountFrontend frontend = CloudMountFrontend::WinFsp;
    std::string account_key;
    std::filesystem::path mount_point;
    std::filesystem::path cache_dir;
    bool writable = false;
};

struct CloudMountStatus {
    CloudMountFrontend frontend = CloudMountFrontend::WinFsp;
    std::string account_key;
    std::filesystem::path mount_point;
    std::filesystem::path cache_dir;
    bool running = false;
    bool writable = false;
    bool dependency_available = false;
    std::string message;
};

class ICloudMountSession {
public:
    virtual ~ICloudMountSession() = default;

    [[nodiscard]] virtual Result<void> start(CloudVfs::CloudVfs &vfs, const CloudMountRequest &request) = 0;
    [[nodiscard]] virtual Result<void> stop() = 0;
    [[nodiscard]] virtual CloudMountStatus status() const = 0;
};

class ICloudMountAdapter {
public:
    virtual ~ICloudMountAdapter() = default;

    [[nodiscard]] virtual CloudMountFrontend frontend() const = 0;
    [[nodiscard]] virtual bool available() const = 0;
    [[nodiscard]] virtual Result<std::unique_ptr<ICloudMountSession>> create_session() = 0;
};

[[nodiscard]] std::unique_ptr<ICloudMountAdapter> create_winfsp_cloud_mount_adapter();
[[nodiscard]] std::unique_ptr<ICloudMountAdapter> create_cloud_files_mount_adapter();
[[nodiscard]] std::unique_ptr<ICloudMountAdapter> create_fuse_cloud_mount_adapter();

} // namespace UBAANext::Runtime
