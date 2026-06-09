#pragma once

#include <UBAANext/Runtime/CloudMountAdapter.hpp>

#include <memory>
#include <mutex>
#include <vector>

namespace UBAANext::Runtime {

class CloudMountManager {
public:
    explicit CloudMountManager(CloudVfs::CloudVfs *vfs = nullptr);
    ~CloudMountManager();

    CloudMountManager(const CloudMountManager &) = delete;
    CloudMountManager &operator=(const CloudMountManager &) = delete;
    CloudMountManager(CloudMountManager &&) = delete;
    CloudMountManager &operator=(CloudMountManager &&) = delete;

    void set_vfs(CloudVfs::CloudVfs &vfs);
    void register_adapter(std::unique_ptr<ICloudMountAdapter> adapter);
    [[nodiscard]] Result<CloudMountStatus> start(const CloudMountRequest &request);
    [[nodiscard]] Result<CloudMountStatus> stop(CloudMountFrontend frontend);
    void stop_all();
    [[nodiscard]] std::vector<CloudMountStatus> statuses() const;
    [[nodiscard]] bool has_writable_frontend(const std::string &account_key, const std::filesystem::path &cache_dir) const;
    [[nodiscard]] static std::string frontend_name(CloudMountFrontend frontend);
    [[nodiscard]] static bool dependency_available(CloudMountFrontend frontend);

private:
    [[nodiscard]] ICloudMountAdapter *adapter_for_locked(CloudMountFrontend frontend) const;
    [[nodiscard]] ICloudMountSession *session_for_locked(CloudMountFrontend frontend) const;
    [[nodiscard]] std::vector<CloudMountStatus> statuses_locked() const;
    [[nodiscard]] bool has_writable_frontend_locked(const std::string &account_key, const std::filesystem::path &cache_dir) const;

    mutable std::mutex m_mutex;
    CloudVfs::CloudVfs *m_vfs = nullptr;
    std::vector<std::unique_ptr<ICloudMountAdapter>> m_adapters;
    std::vector<std::unique_ptr<ICloudMountSession>> m_sessions;
    std::vector<CloudMountStatus> m_statuses;
};

} // namespace UBAANext::Runtime
