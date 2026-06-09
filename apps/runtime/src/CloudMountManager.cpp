#include <UBAANext/Runtime/CloudMountManager.hpp>

#include <algorithm>
#include <utility>

namespace UBAANext::Runtime {
namespace {

CloudMountStatus request_status(const CloudMountRequest &request, bool dependency_available) {
    CloudMountStatus status;
    status.frontend = request.frontend;
    status.account_key = request.account_key;
    status.mount_point = request.mount_point;
    status.cache_dir = request.cache_dir;
    status.running = false;
    status.writable = false;
    status.dependency_available = dependency_available;
    return status;
}

CloudMountStatus unavailable_status(const CloudMountRequest &request) {
    auto status = request_status(request, false);
    status.message = CloudMountManager::frontend_name(request.frontend) + " dependency is not available in this build/runtime";
    return status;
}

void upsert_status(std::vector<CloudMountStatus> &statuses, CloudMountStatus status) {
    auto existing = std::find_if(statuses.begin(), statuses.end(), [&](const CloudMountStatus &item) {
        return item.frontend == status.frontend;
    });
    if (existing == statuses.end()) {
        statuses.push_back(std::move(status));
    } else {
        *existing = std::move(status);
    }
}

} // namespace

CloudMountManager::CloudMountManager(CloudVfs::CloudVfs *vfs)
    : m_vfs(vfs) {}

CloudMountManager::~CloudMountManager() {
    stop_all();
}

void CloudMountManager::set_vfs(CloudVfs::CloudVfs &vfs) {
    std::lock_guard lock(m_mutex);
    if (m_vfs != &vfs) {
        for (const auto &session : m_sessions) {
            auto stopped = session->stop();
            (void)stopped;
            upsert_status(m_statuses, session->status());
        }
        m_sessions.clear();
    }
    m_vfs = &vfs;
}

void CloudMountManager::register_adapter(std::unique_ptr<ICloudMountAdapter> adapter) {
    if (!adapter) return;
    std::lock_guard lock(m_mutex);
    auto frontend = adapter->frontend();
    auto existing_session = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const auto &item) {
        return item->status().frontend == frontend;
    });
    if (existing_session != m_sessions.end()) {
        auto stopped = (*existing_session)->stop();
        (void)stopped;
        upsert_status(m_statuses, (*existing_session)->status());
        m_sessions.erase(existing_session);
    }
    auto existing = std::find_if(m_adapters.begin(), m_adapters.end(), [&](const auto &item) {
        return item->frontend() == frontend;
    });
    if (existing == m_adapters.end()) {
        m_adapters.push_back(std::move(adapter));
    } else {
        *existing = std::move(adapter);
    }
}

Result<CloudMountStatus> CloudMountManager::start(const CloudMountRequest &request) {
    std::lock_guard lock(m_mutex);
    if (request.account_key.empty()) return make_error(ErrorCode::InvalidArgument, "cloud mount requires account key");
    if (request.mount_point.empty()) return make_error(ErrorCode::InvalidArgument, "cloud mount requires mount point");
    auto *adapter = adapter_for_locked(request.frontend);
    if (!adapter || !adapter->available()) {
        auto status = unavailable_status(request);
        upsert_status(m_statuses, status);
        return make_error(ErrorCode::UnsupportedPlatform, status.message);
    }
    if (!m_vfs) return make_error(ErrorCode::InvalidArgument, "cloud mount requires Cloud VFS");

    auto effective_request = request;
    if (request.writable && has_writable_frontend_locked(request.account_key, request.cache_dir)) {
        auto status = request_status(request, adapter->available());
        status.message = frontend_name(request.frontend) + " writable mount refused because another writable frontend is active";
        upsert_status(m_statuses, status);
        return make_error(ErrorCode::InvalidArgument, status.message);
    }

    auto existing_session = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const auto &item) {
        return item->status().frontend == request.frontend;
    });
    if (existing_session != m_sessions.end()) {
        auto stopped = (*existing_session)->stop();
        if (!stopped) return make_error(stopped.error().code, stopped.error().message);
        upsert_status(m_statuses, (*existing_session)->status());
        m_sessions.erase(existing_session);
    }

    auto session = adapter->create_session();
    if (!session) return make_error(session.error().code, session.error().message);

    auto started = (*session)->start(*m_vfs, effective_request);
    if (!started) return make_error(started.error().code, started.error().message);

    auto status = (*session)->status();

    m_sessions.push_back(std::move(*session));
    upsert_status(m_statuses, status);
    return status;
}

Result<CloudMountStatus> CloudMountManager::stop(CloudMountFrontend frontend) {
    std::lock_guard lock(m_mutex);
    auto *session = session_for_locked(frontend);
    if (!session) return make_error(ErrorCode::InvalidArgument, "mount frontend is not running");
    auto stopped = session->stop();
    if (!stopped) return make_error(stopped.error().code, stopped.error().message);
    auto status = session->status();
    upsert_status(m_statuses, status);
    m_sessions.erase(std::remove_if(m_sessions.begin(), m_sessions.end(), [&](const auto &item) {
                         return item->status().frontend == frontend;
                     }),
                     m_sessions.end());
    return status;
}

void CloudMountManager::stop_all() {
    std::lock_guard lock(m_mutex);
    for (const auto &session : m_sessions) {
        auto stopped = session->stop();
        (void)stopped;
        upsert_status(m_statuses, session->status());
    }
    m_sessions.clear();
}

std::vector<CloudMountStatus> CloudMountManager::statuses() const {
    std::lock_guard lock(m_mutex);
    return statuses_locked();
}

bool CloudMountManager::has_writable_frontend(const std::string &account_key, const std::filesystem::path &cache_dir) const {
    std::lock_guard lock(m_mutex);
    return has_writable_frontend_locked(account_key, cache_dir);
}

std::string CloudMountManager::frontend_name(CloudMountFrontend frontend) {
    switch (frontend) {
    case CloudMountFrontend::WinFsp: return "WinFsp";
    case CloudMountFrontend::CloudFiles: return "Cloud Files";
    case CloudMountFrontend::Fuse: return "FUSE";
    }
    return "Unknown";
}

bool CloudMountManager::dependency_available(CloudMountFrontend frontend) {
    switch (frontend) {
    case CloudMountFrontend::WinFsp:
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        return true;
#else
        return false;
#endif
    case CloudMountFrontend::CloudFiles:
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_CLOUD_FILES)
        return UBAANEXT_ENABLE_CLOUD_FILES;
#else
        return false;
#endif
    case CloudMountFrontend::Fuse:
#if defined(__linux__) && defined(UBAANEXT_ENABLE_FUSE)
        return UBAANEXT_ENABLE_FUSE;
#else
        return false;
#endif
    }
    return false;
}

ICloudMountAdapter *CloudMountManager::adapter_for_locked(CloudMountFrontend frontend) const {
    auto it = std::find_if(m_adapters.begin(), m_adapters.end(), [&](const auto &adapter) {
        return adapter->frontend() == frontend;
    });
    return it == m_adapters.end() ? nullptr : it->get();
}

ICloudMountSession *CloudMountManager::session_for_locked(CloudMountFrontend frontend) const {
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(), [&](const auto &session) {
        return session->status().frontend == frontend;
    });
    return it == m_sessions.end() ? nullptr : it->get();
}

std::vector<CloudMountStatus> CloudMountManager::statuses_locked() const {
    std::vector<CloudMountStatus> statuses;
    for (const auto &adapter : m_adapters) {
        CloudMountStatus status;
        status.frontend = adapter->frontend();
        status.running = false;
        status.writable = false;
        status.dependency_available = adapter->available();
        status.message = adapter->available() ? frontend_name(adapter->frontend()) + " dependency is available"
                                      : frontend_name(adapter->frontend()) + " dependency is not available in this build/runtime";
        upsert_status(statuses, std::move(status));
    }
    for (const auto &status : m_statuses) upsert_status(statuses, status);
    for (const auto &session : m_sessions) upsert_status(statuses, session->status());
    return statuses;
}

bool CloudMountManager::has_writable_frontend_locked(const std::string &account_key, const std::filesystem::path &cache_dir) const {
    auto statuses = statuses_locked();
    return std::any_of(statuses.begin(), statuses.end(), [&](const CloudMountStatus &status) {
        return status.running && status.writable && status.account_key == account_key && status.cache_dir == cache_dir;
    });
}

} // namespace UBAANext::Runtime
