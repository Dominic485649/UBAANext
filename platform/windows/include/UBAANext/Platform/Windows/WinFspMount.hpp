#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/CloudVfs/CloudVfs.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace UBAANext::Platform::Windows {

struct WinFspMountOptions {
    std::filesystem::path mount_point;
};

struct WinFspMountEntry {
    std::string name;
    std::string path;
    bool is_dir = false;
    std::uint64_t size = 0;
    std::string mtime;
};

struct WinFspMountHandle {
    std::string path;
    bool is_dir = false;
};

class WinFspMount {
public:
    WinFspMount();
    ~WinFspMount();

    WinFspMount(const WinFspMount &) = delete;
    WinFspMount &operator=(const WinFspMount &) = delete;

    [[nodiscard]] Result<void> start(CloudVfs::CloudVfs &vfs, WinFspMountOptions options);
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool running() const;
    [[nodiscard]] Result<WinFspMountEntry> stat(const std::string &path) const;
    [[nodiscard]] Result<std::vector<WinFspMountEntry>> list(const std::string &path, bool refresh = false) const;
    [[nodiscard]] Result<WinFspMountHandle> open(const std::string &path) const;
    [[nodiscard]] Result<std::vector<unsigned char>> read(const WinFspMountHandle &handle,
                                                          std::uint64_t offset,
                                                          std::uint64_t length) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace UBAANext::Platform::Windows
