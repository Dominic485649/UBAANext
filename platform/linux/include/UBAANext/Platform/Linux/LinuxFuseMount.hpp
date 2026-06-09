#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/CloudVfs/CloudVfs.hpp>

#include <filesystem>
#include <memory>

namespace UBAANext::Platform::Linux {

struct LinuxFuseMountOptions {
    std::filesystem::path mount_point;
    bool foreground = false;
};

class LinuxFuseMount final {
public:
    LinuxFuseMount();
    ~LinuxFuseMount();

    LinuxFuseMount(const LinuxFuseMount &) = delete;
    LinuxFuseMount &operator=(const LinuxFuseMount &) = delete;

    [[nodiscard]] Result<void> start(CloudVfs::CloudVfs &vfs, LinuxFuseMountOptions options);
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool running() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace UBAANext::Platform::Linux
