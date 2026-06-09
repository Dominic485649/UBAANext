#include <UBAANext/Platform/Windows/WinFspMount.hpp>

#include <algorithm>
#include <cstring>
#include <functional>
#include <new>
#include <utility>

#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winfsp/winfsp.h>

#include <cwchar>
#endif

namespace UBAANext::Platform::Windows {
namespace {

WinFspMountEntry entry_from_node(const CloudVfs::CloudVfsNode &node) {
    WinFspMountEntry entry;
    entry.name = node.name;
    entry.path = node.path.value;
    entry.is_dir = node.is_dir;
    entry.size = node.size;
    entry.mtime = node.mtime;
    return entry;
}

#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP

std::wstring widen(const std::string &value) {
    if (value.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string narrow(PWSTR value) {
    if (!value || *value == L'\0') return "/";
    const auto length = static_cast<int>(std::wcslen(value));
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value, length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "/";
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, length, out.data(), size, nullptr, nullptr);
    std::replace(out.begin(), out.end(), '\\', '/');
    if (out.empty() || out[0] != '/') out.insert(out.begin(), '/');
    return out;
}

NTSTATUS error_to_status(ErrorCode code) {
    switch (code) {
    case ErrorCode::InvalidArgument: return STATUS_OBJECT_NAME_NOT_FOUND;
    case ErrorCode::UnsupportedPlatform: return STATUS_INVALID_DEVICE_REQUEST;
    case ErrorCode::NetworkError: return STATUS_UNSUCCESSFUL;
    case ErrorCode::AuthenticationFailed: return STATUS_ACCESS_DENIED;
    case ErrorCode::ParseError: return STATUS_UNSUCCESSFUL;
    case ErrorCode::StorageError: return STATUS_UNSUCCESSFUL;
    }
    return STATUS_UNSUCCESSFUL;
}

bool wants_write_access(UINT32 granted_access) {
    constexpr UINT32 write_mask = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES |
                                  DELETE | WRITE_DAC | WRITE_OWNER;
    return (granted_access & write_mask) != 0;
}

std::uint64_t index_for_path(const std::string &path) {
    auto value = std::hash<std::string>{}(path);
    if (value == 0) value = 1;
    return static_cast<std::uint64_t>(value);
}

void fill_file_info(const CloudVfs::CloudVfsNode &node, FSP_FSCTL_FILE_INFO &info) {
    std::memset(&info, 0, sizeof(info));
    info.FileAttributes = node.is_dir ? FILE_ATTRIBUTE_DIRECTORY : (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY);
    info.AllocationSize = node.size;
    info.FileSize = node.size;
    info.IndexNumber = index_for_path(node.path.value);
    info.HardLinks = 1;
}

bool marker_passed(PWSTR marker, const std::wstring &name, bool &passed) {
    if (passed || !marker || *marker == L'\0') return true;
    if (name == marker) passed = true;
    return false;
}

bool add_dir_info(const std::wstring &name,
                  const CloudVfs::CloudVfsNode &node,
                  void *buffer,
                  ULONG length,
                  PULONG bytes_transferred) {
    const auto name_bytes = name.size() * sizeof(WCHAR);
    std::vector<unsigned char> storage(sizeof(FSP_FSCTL_DIR_INFO) + name_bytes);
    auto *dir_info = reinterpret_cast<FSP_FSCTL_DIR_INFO *>(storage.data());
    std::memset(dir_info, 0, sizeof(FSP_FSCTL_DIR_INFO));
    dir_info->Size = static_cast<UINT16>(sizeof(FSP_FSCTL_DIR_INFO) + name_bytes);
    fill_file_info(node, dir_info->FileInfo);
    if (name_bytes > 0) std::memcpy(dir_info->FileNameBuf, name.data(), name_bytes);
    return FspFileSystemAddDirInfo(dir_info, buffer, length, bytes_transferred) != 0;
}

struct WinFspFileContext {
    CloudVfs::CloudVfsNode node;
};

#endif

} // namespace

class WinFspMount::Impl final {
public:
    ~Impl() { stop(); }

    Result<void> start(CloudVfs::CloudVfs &vfs, WinFspMountOptions options) {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        if (m_file_system) return make_error(ErrorCode::InvalidArgument, "WinFsp mount is already running");
        if (options.mount_point.empty()) return make_error(ErrorCode::InvalidArgument, "WinFsp mount requires mount point");

        auto root = vfs.root();
        if (!root) root = vfs.load_user_root();
        if (!root) return make_error(root.error().code, root.error().message);
        auto children = vfs.list("/", false);
        if (!children) return make_error(children.error().code, children.error().message);

        FSP_FSCTL_VOLUME_PARAMS params{};
        params.Version = sizeof(FSP_FSCTL_VOLUME_PARAMS);
        params.SectorSize = 512;
        params.SectorsPerAllocationUnit = 1;
        params.VolumeCreationTime = 0;
        params.VolumeSerialNumber = 0x55424141;
        params.FileInfoTimeout = 1000;
        params.CaseSensitiveSearch = 0;
        params.CasePreservedNames = 1;
        params.UnicodeOnDisk = 1;
        params.PersistentAcls = 0;
        std::wcscpy(params.FileSystemName, L"UBAANext");

        auto interface = FSP_FILE_SYSTEM_INTERFACE{};
        interface.GetVolumeInfo = get_volume_info;
        interface.GetSecurityByName = get_security_by_name;
        interface.Create = create;
        interface.Open = open;
        interface.Cleanup = cleanup;
        interface.Close = close;
        interface.Read = read;
        interface.Write = write;
        interface.Flush = flush;
        interface.GetFileInfo = get_file_info;
        interface.SetBasicInfo = set_basic_info;
        interface.SetFileSize = set_file_size;
        interface.CanDelete = can_delete;
        interface.Rename = rename;
        interface.GetSecurity = get_security;
        interface.ReadDirectory = read_directory;

        FSP_FILE_SYSTEM *file_system = nullptr;
        auto status = FspFileSystemCreate(const_cast<PWSTR>(FSP_FSCTL_DISK_DEVICE_NAME), &params, &interface, &file_system);
        if (!NT_SUCCESS(status)) return make_error(ErrorCode::StorageError, "failed to create WinFsp file system");

        m_file_system = file_system;
        m_file_system->UserContext = this;
        m_vfs = &vfs;
        m_options = std::move(options);
        m_mount_point = m_options.mount_point.wstring();

        status = FspFileSystemSetMountPoint(m_file_system, m_mount_point.data());
        if (!NT_SUCCESS(status)) {
            stop();
            return make_error(ErrorCode::StorageError, "failed to set WinFsp mount point");
        }

        status = FspFileSystemStartDispatcher(m_file_system, 0);
        if (!NT_SUCCESS(status)) {
            stop();
            return make_error(ErrorCode::StorageError, "failed to start WinFsp dispatcher");
        }
        return {};
#else
        (void)vfs;
        (void)options;
        return make_error(ErrorCode::UnsupportedPlatform, "WinFsp dependency is not available in this build/runtime");
#endif
    }

    Result<void> stop() {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        if (m_file_system) {
            FspFileSystemStopDispatcher(m_file_system);
            FspFileSystemRemoveMountPoint(m_file_system);
            FspFileSystemDelete(m_file_system);
            m_file_system = nullptr;
        }
#endif
        m_vfs = nullptr;
        m_options = {};
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        m_mount_point.clear();
#endif
        return {};
    }

    bool running() const {
#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
        return m_file_system != nullptr;
#else
        return false;
#endif
    }

    Result<WinFspMountEntry> stat(const std::string &path) const {
        auto vfs = require_vfs();
        if (!vfs) return make_error(vfs.error().code, vfs.error().message);
        auto node = (*vfs)->lookup(path);
        if (!node) return make_error(node.error().code, node.error().message);
        return entry_from_node(*node);
    }

    Result<std::vector<WinFspMountEntry>> list(const std::string &path, bool refresh) const {
        auto vfs = require_vfs();
        if (!vfs) return make_error(vfs.error().code, vfs.error().message);
        auto nodes = (*vfs)->list(path, refresh);
        if (!nodes) return make_error(nodes.error().code, nodes.error().message);
        std::vector<WinFspMountEntry> entries;
        entries.reserve(nodes->size());
        for (const auto &node : *nodes) entries.push_back(entry_from_node(node));
        return entries;
    }

    Result<WinFspMountHandle> open(const std::string &path) const {
        auto entry = stat(path);
        if (!entry) return make_error(entry.error().code, entry.error().message);
        WinFspMountHandle handle;
        handle.path = entry->path;
        handle.is_dir = entry->is_dir;
        return handle;
    }

    Result<std::vector<unsigned char>> read(const WinFspMountHandle &handle,
                                            std::uint64_t offset,
                                            std::uint64_t length) const {
        auto vfs = require_vfs();
        if (!vfs) return make_error(vfs.error().code, vfs.error().message);
        if (handle.path.empty()) return make_error(ErrorCode::InvalidArgument, "WinFsp read requires open handle");
        if (handle.is_dir) return make_error(ErrorCode::InvalidArgument, "WinFsp cannot read directory content");
        auto bytes = (*vfs)->read(handle.path, offset, length);
        if (!bytes) return make_error(bytes.error().code, bytes.error().message);
        return *bytes;
    }

private:
    Result<CloudVfs::CloudVfs *> require_vfs() const {
        if (!running() || !m_vfs) return make_error(ErrorCode::InvalidArgument, "WinFsp mount is not running");
        return m_vfs;
    }

#if defined(_WIN32) && defined(UBAANEXT_ENABLE_WINFSP) && UBAANEXT_ENABLE_WINFSP
    static Impl &self(FSP_FILE_SYSTEM *file_system) {
        return *static_cast<Impl *>(file_system->UserContext);
    }

    NTSTATUS lookup_node(const std::string &path, CloudVfs::CloudVfsNode &node) const {
        if (!m_vfs) return STATUS_INVALID_DEVICE_REQUEST;
        auto result = m_vfs->lookup(path);
        if (!result) return error_to_status(result.error().code);
        node = std::move(*result);
        return STATUS_SUCCESS;
    }

    static NTSTATUS get_volume_info(FSP_FILE_SYSTEM *, FSP_FSCTL_VOLUME_INFO *volume_info) {
        volume_info->TotalSize = 1ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        volume_info->FreeSize = volume_info->TotalSize;
        const std::wstring label = L"UBAANext";
        volume_info->VolumeLabelLength = static_cast<UINT16>(label.size() * sizeof(WCHAR));
        std::wmemcpy(volume_info->VolumeLabel, label.c_str(), label.size());
        return STATUS_SUCCESS;
    }

    static NTSTATUS get_security_by_name(FSP_FILE_SYSTEM *file_system,
                                          PWSTR file_name,
                                          PUINT32 file_attributes,
                                          PSECURITY_DESCRIPTOR,
                                          SIZE_T *security_descriptor_size) {
        CloudVfs::CloudVfsNode node;
        auto status = self(file_system).lookup_node(narrow(file_name), node);
        if (!NT_SUCCESS(status)) return status;
        if (file_attributes) {
            *file_attributes = node.is_dir ? FILE_ATTRIBUTE_DIRECTORY : (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_READONLY);
        }
        if (security_descriptor_size) *security_descriptor_size = 0;
        return STATUS_SUCCESS;
    }

    static NTSTATUS create(FSP_FILE_SYSTEM *,
                           PWSTR,
                           UINT32,
                           UINT32,
                           UINT32,
                           PSECURITY_DESCRIPTOR,
                           UINT64,
                           PVOID *,
                           FSP_FSCTL_FILE_INFO *) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS open(FSP_FILE_SYSTEM *file_system,
                         PWSTR file_name,
                         UINT32,
                         UINT32 granted_access,
                         PVOID *file_context,
                         FSP_FSCTL_FILE_INFO *file_info) {
        if (wants_write_access(granted_access)) return STATUS_ACCESS_DENIED;
        CloudVfs::CloudVfsNode node;
        auto status = self(file_system).lookup_node(narrow(file_name), node);
        if (!NT_SUCCESS(status)) return status;
        auto *context = new (std::nothrow) WinFspFileContext{std::move(node)};
        if (!context) return STATUS_INSUFFICIENT_RESOURCES;
        *file_context = context;
        fill_file_info(context->node, *file_info);
        return STATUS_SUCCESS;
    }

    static void cleanup(FSP_FILE_SYSTEM *, PVOID, PWSTR, ULONG) {}

    static void close(FSP_FILE_SYSTEM *, PVOID file_context) {
        delete static_cast<WinFspFileContext *>(file_context);
    }

    static NTSTATUS read(FSP_FILE_SYSTEM *file_system,
                         PVOID file_context,
                         PVOID buffer,
                         UINT64 offset,
                         ULONG length,
                         PULONG bytes_transferred) {
        auto *context = static_cast<WinFspFileContext *>(file_context);
        if (!context) return STATUS_INVALID_DEVICE_REQUEST;
        if (context->node.is_dir) return STATUS_INVALID_DEVICE_REQUEST;
        if (offset >= context->node.size) {
            *bytes_transferred = 0;
            return STATUS_END_OF_FILE;
        }
        const auto count = std::min<std::uint64_t>(length, context->node.size - offset);
        auto bytes = self(file_system).m_vfs->read(context->node.path.value, offset, count);
        if (!bytes) return error_to_status(bytes.error().code);
        std::memcpy(buffer, bytes->data(), bytes->size());
        *bytes_transferred = static_cast<ULONG>(bytes->size());
        return STATUS_SUCCESS;
    }

    static NTSTATUS write(FSP_FILE_SYSTEM *,
                          PVOID,
                          PVOID,
                          UINT64,
                          ULONG,
                          BOOLEAN,
                          BOOLEAN,
                          PULONG,
                          FSP_FSCTL_FILE_INFO *) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS flush(FSP_FILE_SYSTEM *, PVOID file_context, FSP_FSCTL_FILE_INFO *file_info) {
        auto *context = static_cast<WinFspFileContext *>(file_context);
        if (context && file_info) fill_file_info(context->node, *file_info);
        return STATUS_SUCCESS;
    }

    static NTSTATUS get_file_info(FSP_FILE_SYSTEM *, PVOID file_context, FSP_FSCTL_FILE_INFO *file_info) {
        auto *context = static_cast<WinFspFileContext *>(file_context);
        if (!context) return STATUS_INVALID_DEVICE_REQUEST;
        fill_file_info(context->node, *file_info);
        return STATUS_SUCCESS;
    }

    static NTSTATUS set_basic_info(FSP_FILE_SYSTEM *, PVOID, UINT32, UINT64, UINT64, UINT64, UINT64, FSP_FSCTL_FILE_INFO *) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS set_file_size(FSP_FILE_SYSTEM *, PVOID, UINT64, BOOLEAN, FSP_FSCTL_FILE_INFO *) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS can_delete(FSP_FILE_SYSTEM *, PVOID, PWSTR) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS rename(FSP_FILE_SYSTEM *, PVOID, PWSTR, PWSTR, BOOLEAN) {
        return STATUS_ACCESS_DENIED;
    }

    static NTSTATUS get_security(FSP_FILE_SYSTEM *, PVOID, PSECURITY_DESCRIPTOR, SIZE_T *security_descriptor_size) {
        if (security_descriptor_size) *security_descriptor_size = 0;
        return STATUS_SUCCESS;
    }

    static NTSTATUS read_directory(FSP_FILE_SYSTEM *file_system,
                                   PVOID file_context,
                                   PWSTR,
                                   PWSTR marker,
                                   PVOID buffer,
                                   ULONG length,
                                   PULONG bytes_transferred) {
        auto *context = static_cast<WinFspFileContext *>(file_context);
        if (!context) return STATUS_INVALID_DEVICE_REQUEST;
        if (!context->node.is_dir) return STATUS_NOT_A_DIRECTORY;
        auto entries = self(file_system).m_vfs->list(context->node.path.value);
        if (!entries) return error_to_status(entries.error().code);

        bool passed_marker = marker == nullptr || *marker == L'\0';
        auto try_add = [&](const std::wstring &name, const CloudVfs::CloudVfsNode &node) -> bool {
            if (!marker_passed(marker, name, passed_marker)) return true;
            if (marker && *marker != L'\0' && name == marker) return true;
            return add_dir_info(name, node, buffer, length, bytes_transferred);
        };

        if (context->node.path.value != "/") {
            if (!try_add(L".", context->node)) return STATUS_SUCCESS;
            if (!try_add(L"..", context->node)) return STATUS_SUCCESS;
        }
        for (const auto &entry : *entries) {
            auto name = widen(entry.name);
            if (!try_add(name, entry)) return STATUS_SUCCESS;
        }
        FspFileSystemAddDirInfo(nullptr, buffer, length, bytes_transferred);
        return STATUS_SUCCESS;
    }

    FSP_FILE_SYSTEM *m_file_system = nullptr;
    std::wstring m_mount_point;
#endif

    CloudVfs::CloudVfs *m_vfs = nullptr;
    WinFspMountOptions m_options;
};

WinFspMount::WinFspMount() : m_impl(std::make_unique<Impl>()) {}
WinFspMount::~WinFspMount() = default;

Result<void> WinFspMount::start(CloudVfs::CloudVfs &vfs, WinFspMountOptions options) {
    return m_impl->start(vfs, std::move(options));
}

Result<void> WinFspMount::stop() {
    return m_impl->stop();
}

bool WinFspMount::running() const {
    return m_impl->running();
}

Result<WinFspMountEntry> WinFspMount::stat(const std::string &path) const {
    return m_impl->stat(path);
}

Result<std::vector<WinFspMountEntry>> WinFspMount::list(const std::string &path, bool refresh) const {
    return m_impl->list(path, refresh);
}

Result<WinFspMountHandle> WinFspMount::open(const std::string &path) const {
    return m_impl->open(path);
}

Result<std::vector<unsigned char>> WinFspMount::read(const WinFspMountHandle &handle,
                                                     std::uint64_t offset,
                                                     std::uint64_t length) const {
    return m_impl->read(handle, offset, length);
}

} // namespace UBAANext::Platform::Windows
