#include <UBAANext/Runtime/CloudMountAdapter.hpp>
#include <UBAANext/Runtime/CloudMountManager.hpp>

#if defined(_WIN32) && defined(UBAANEXT_ENABLE_CLOUD_FILES) && UBAANEXT_ENABLE_CLOUD_FILES

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <cfapi.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace UBAANext::Runtime {
namespace {

std::wstring widen(const std::filesystem::path &path) {
    return path.wstring();
}

std::wstring widen_text(const std::string &text) {
    if (text.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return std::wstring(text.begin(), text.end());
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::string hresult_message(HRESULT hr, const char *operation) {
    std::ostringstream out;
    out << operation << " failed with HRESULT 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return out.str();
}

ErrorCode filesystem_error_code() {
    return ErrorCode::StorageError;
}

CF_FS_METADATA fs_metadata(const CloudVfs::CloudVfsNode &node) {
    CF_FS_METADATA metadata{};
    metadata.BasicInfo.FileAttributes = node.is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    metadata.FileSize.QuadPart = static_cast<LONGLONG>(node.size);
    return metadata;
}

CF_PLACEHOLDER_CREATE_INFO placeholder_info(const CloudVfs::CloudVfsNode &node,
                                            std::wstring &name,
                                            std::string &identity) {
    name = widen_text(node.name);
    identity = node.path.value;
    CF_PLACEHOLDER_CREATE_INFO info{};
    info.RelativeFileName = name.c_str();
    info.FsMetadata = fs_metadata(node);
    info.FileIdentity = identity.data();
    info.FileIdentityLength = static_cast<DWORD>(identity.size());
    info.Flags = CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
    return info;
}

std::filesystem::path relative_path_from_callback(const CF_CALLBACK_INFO &info, const std::filesystem::path &sync_root) {
    if (!info.NormalizedPath) return {};
    std::error_code ec;
    auto full = std::filesystem::weakly_canonical(std::filesystem::path(info.NormalizedPath), ec);
    if (ec) {
        ec.clear();
        full = std::filesystem::absolute(std::filesystem::path(info.NormalizedPath), ec);
    }
    auto root = std::filesystem::weakly_canonical(sync_root, ec);
    if (ec) {
        ec.clear();
        root = std::filesystem::absolute(sync_root, ec);
    }
    auto relative = full.lexically_relative(root);
    if (relative.empty()) return {};
    auto first = relative.begin();
    if (first != relative.end() && *first == "..") return {};
    return relative;
}

std::string vfs_path_from_relative(const std::filesystem::path &relative) {
    auto generic = relative.generic_string();
    if (generic.empty() || generic == ".") return "/";
    return "/" + generic;
}

class CloudFilesMountSession final : public ICloudMountSession {
public:
    [[nodiscard]] Result<void> start(CloudVfs::CloudVfs &vfs, const CloudMountRequest &request) override {
        if (request.writable) {
            return make_error(ErrorCode::UnsupportedPlatform, "Cloud Files writable mount is not implemented yet");
        }

        m_vfs = &vfs;
        m_status.frontend = CloudMountFrontend::CloudFiles;
        m_status.account_key = request.account_key;
        m_status.mount_point = request.mount_point;
        m_status.cache_dir = request.cache_dir;
        m_status.writable = false;
        m_status.dependency_available = true;

        std::error_code ec;
        std::filesystem::create_directories(request.mount_point, ec);
        if (ec) return make_error(filesystem_error_code(), "Cloud Files mount directory create failed: " + ec.message());
        std::filesystem::create_directories(request.cache_dir, ec);
        if (!request.cache_dir.empty() && ec) return make_error(filesystem_error_code(), "Cloud Files cache directory create failed: " + ec.message());

        auto root = m_vfs->root();
        if (!root) {
            root = m_vfs->load_user_root();
            if (!root) return make_error(root.error().code, root.error().message);
        }

        auto registered = register_sync_root(request);
        if (!registered) return make_error(registered.error().code, registered.error().message);

        CF_CONNECTION_KEY key{};
        CF_CALLBACK_REGISTRATION callbacks[] = {
            {CF_CALLBACK_TYPE_FETCH_DATA, &CloudFilesMountSession::fetch_data},
            {CF_CALLBACK_TYPE_FETCH_PLACEHOLDERS, &CloudFilesMountSession::fetch_placeholders},
            {CF_CALLBACK_TYPE_NOTIFY_FILE_OPEN_COMPLETION, &CloudFilesMountSession::file_open_completed},
            {CF_CALLBACK_TYPE_NONE, nullptr},
        };
        const auto connect = CfConnectSyncRoot(widen(request.mount_point).c_str(), callbacks, this, CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO, &key);
        if (FAILED(connect)) return make_error(ErrorCode::UnsupportedPlatform, hresult_message(connect, "CfConnectSyncRoot"));
        m_connection_key = key;
        m_connected = true;

        auto enumerated = materialize_placeholders("/", request.mount_point);
        if (!enumerated) {
            auto stopped = stop();
            (void)stopped;
            return make_error(enumerated.error().code, enumerated.error().message);
        }

        m_status.running = true;
        m_status.message = "Cloud Files mount started read-only";
        return {};
    }

    [[nodiscard]] Result<void> stop() override {
        if (m_connected) {
            CfDisconnectSyncRoot(m_connection_key);
            m_connected = false;
        }
        m_status.running = false;
        m_status.writable = false;
        m_status.message = "Cloud Files mount stopped";
        return {};
    }

    [[nodiscard]] CloudMountStatus status() const override {
        return m_status;
    }

private:
    [[nodiscard]] Result<void> register_sync_root(const CloudMountRequest &request) {
        CF_SYNC_REGISTRATION registration{};
        registration.StructSize = sizeof(registration);
        auto provider_name = widen_text("UBAANext");
        auto provider_version = widen_text("0.4.0");
        registration.ProviderName = provider_name.c_str();
        registration.ProviderVersion = provider_version.c_str();
        registration.SyncRootIdentity = request.account_key.data();
        registration.SyncRootIdentityLength = static_cast<DWORD>(request.account_key.size());
        registration.FileIdentity = request.account_key.data();
        registration.FileIdentityLength = static_cast<DWORD>(request.account_key.size());

        CF_SYNC_POLICIES policies{};
        policies.StructSize = sizeof(policies);
        policies.Hydration.Primary = CF_HYDRATION_POLICY_PROGRESSIVE;
        policies.Hydration.Modifier = CF_HYDRATION_POLICY_MODIFIER_AUTO_DEHYDRATION_ALLOWED;
        policies.Population.Primary = CF_POPULATION_POLICY_PARTIAL;
        policies.InSync = CF_INSYNC_POLICY_TRACK_FILE_ALL;
        policies.HardLink = CF_HARDLINK_POLICY_NONE;
        policies.PlaceholderManagement = CF_PLACEHOLDER_MANAGEMENT_POLICY_DEFAULT;

        const auto hr = CfRegisterSyncRoot(widen(request.mount_point).c_str(), &registration, &policies, CF_REGISTER_FLAG_UPDATE);
        if (FAILED(hr)) return make_error(ErrorCode::UnsupportedPlatform, hresult_message(hr, "CfRegisterSyncRoot"));
        return {};
    }

    [[nodiscard]] Result<void> materialize_placeholders(const std::string &vfs_path, const std::filesystem::path &local_dir) {
        auto children = m_vfs->list(vfs_path, false);
        if (!children) return make_error(children.error().code, children.error().message);

        for (const auto &child : *children) {
            std::wstring name;
            std::string identity;
            auto info = placeholder_info(child, name, identity);
            auto local_path = local_dir / name;
            auto hr = CfCreatePlaceholders(widen(local_dir).c_str(), &info, 1, CF_CREATE_FLAG_NONE, nullptr);
            if (FAILED(hr)) return make_error(filesystem_error_code(), hresult_message(hr, "CfCreatePlaceholders"));
            if (FAILED(info.Result)) return make_error(filesystem_error_code(), hresult_message(info.Result, "CfCreatePlaceholders item"));
            if (child.is_dir) {
                std::error_code ec;
                std::filesystem::create_directories(local_path, ec);
            }
        }
        return {};
    }

    void hydrate(const CF_CALLBACK_INFO &info, const CF_CALLBACK_PARAMETERS &parameters) {
        CF_OPERATION_INFO operation{};
        operation.StructSize = sizeof(operation);
        operation.Type = CF_OPERATION_TYPE_TRANSFER_DATA;
        operation.ConnectionKey = info.ConnectionKey;
        operation.TransferKey = info.TransferKey;
        operation.RequestKey = info.RequestKey;

        CF_OPERATION_PARAMETERS op{};
        op.ParamSize = sizeof(op);
        op.TransferData.Offset = parameters.FetchData.RequiredFileOffset;
        op.TransferData.Length = parameters.FetchData.RequiredLength;

        auto relative = relative_path_from_callback(info, m_status.mount_point);
        auto bytes = m_vfs->read(vfs_path_from_relative(relative),
                                 static_cast<std::uint64_t>(parameters.FetchData.RequiredFileOffset.QuadPart),
                                 static_cast<std::uint64_t>(parameters.FetchData.RequiredLength.QuadPart));
        if (!bytes) {
            op.TransferData.Length.QuadPart = 0;
            op.TransferData.CompletionStatus = static_cast<NTSTATUS>(0xC0000001L);
            CfExecute(&operation, &op);
            return;
        }
        op.TransferData.Buffer = bytes->data();
        op.TransferData.Length.QuadPart = static_cast<LONGLONG>(bytes->size());
        op.TransferData.CompletionStatus = static_cast<NTSTATUS>(0);
        CfExecute(&operation, &op);
    }

    void populate(const CF_CALLBACK_INFO &info) {
        CF_OPERATION_INFO operation{};
        operation.StructSize = sizeof(operation);
        operation.Type = CF_OPERATION_TYPE_TRANSFER_PLACEHOLDERS;
        operation.ConnectionKey = info.ConnectionKey;
        operation.TransferKey = info.TransferKey;
        operation.RequestKey = info.RequestKey;

        CF_OPERATION_PARAMETERS op{};
        op.ParamSize = sizeof(op);
        op.TransferPlaceholders.CompletionStatus = static_cast<NTSTATUS>(0);
        op.TransferPlaceholders.PlaceholderTotalCount.QuadPart = 0;

        auto children = m_vfs->list(vfs_path_from_relative(relative_path_from_callback(info, m_status.mount_point)), false);
        if (!children) {
            op.TransferPlaceholders.CompletionStatus = static_cast<NTSTATUS>(0xC0000001L);
            (void)CfExecute(&operation, &op);
            return;
        }

        std::vector<std::wstring> names;
        std::vector<std::string> identities;
        std::vector<CF_PLACEHOLDER_CREATE_INFO> placeholders;
        names.reserve(children->size());
        identities.reserve(children->size());
        placeholders.reserve(children->size());
        for (const auto &child : *children) {
            names.emplace_back();
            identities.emplace_back();
            placeholders.push_back(placeholder_info(child, names.back(), identities.back()));
        }

        op.TransferPlaceholders.Flags = CF_OPERATION_TRANSFER_PLACEHOLDERS_FLAG_NONE;
        op.TransferPlaceholders.PlaceholderTotalCount.QuadPart = static_cast<LONGLONG>(placeholders.size());
        op.TransferPlaceholders.PlaceholderArray = placeholders.empty() ? nullptr : placeholders.data();
        op.TransferPlaceholders.PlaceholderCount = static_cast<DWORD>(placeholders.size());
        op.TransferPlaceholders.EntriesProcessed = 0;
        (void)CfExecute(&operation, &op);
    }

    static void CALLBACK fetch_data(const CF_CALLBACK_INFO *info, const CF_CALLBACK_PARAMETERS *parameters) {
        if (!info || !parameters || !info->CallbackContext) return;
        static_cast<CloudFilesMountSession *>(info->CallbackContext)->hydrate(*info, *parameters);
    }

    static void CALLBACK fetch_placeholders(const CF_CALLBACK_INFO *info, const CF_CALLBACK_PARAMETERS *parameters) {
        (void)parameters;
        if (!info || !info->CallbackContext) return;
        static_cast<CloudFilesMountSession *>(info->CallbackContext)->populate(*info);
    }

    static void CALLBACK file_open_completed(const CF_CALLBACK_INFO *info, const CF_CALLBACK_PARAMETERS *parameters) {
        (void)info;
        (void)parameters;
    }

    CloudVfs::CloudVfs *m_vfs = nullptr;
    CloudMountStatus m_status;
    CF_CONNECTION_KEY m_connection_key{};
    bool m_connected = false;
};

class CloudFilesMountAdapter final : public ICloudMountAdapter {
public:
    [[nodiscard]] CloudMountFrontend frontend() const override {
        return CloudMountFrontend::CloudFiles;
    }

    [[nodiscard]] bool available() const override {
        return CloudMountManager::dependency_available(CloudMountFrontend::CloudFiles);
    }

    [[nodiscard]] Result<std::unique_ptr<ICloudMountSession>> create_session() override {
        return std::unique_ptr<ICloudMountSession>(std::make_unique<CloudFilesMountSession>());
    }
};

} // namespace

std::unique_ptr<ICloudMountAdapter> create_cloud_files_mount_adapter() {
    return std::make_unique<CloudFilesMountAdapter>();
}

} // namespace UBAANext::Runtime

#endif
