#include <UBAANext/Platform/Linux/LinuxFuseMount.hpp>

#if defined(__linux__) && defined(UBAANEXT_ENABLE_FUSE) && UBAANEXT_ENABLE_FUSE

#define FUSE_USE_VERSION 31
#include <fuse3/fuse_lowlevel.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace UBAANext::Platform::Linux {
namespace {

std::uint64_t read_size(const CloudVfs::CloudVfsNode &node, off_t offset, size_t size) {
    if (offset < 0) return 0;
    if (static_cast<std::uint64_t>(offset) >= node.size) return 0;
    return std::min<std::uint64_t>(size, node.size - static_cast<std::uint64_t>(offset));
}

int error_to_errno(ErrorCode code) {
    switch (code) {
    case ErrorCode::InvalidArgument: return ENOENT;
    case ErrorCode::UnsupportedPlatform: return ENOSYS;
    case ErrorCode::NetworkError: return EIO;
    case ErrorCode::AuthenticationFailed: return EACCES;
    case ErrorCode::ParseError: return EIO;
    case ErrorCode::StorageError: return EIO;
    }
    return EIO;
}

class SessionGuard final {
public:
    explicit SessionGuard(fuse_session *session) : m_session(session) {}
    ~SessionGuard() {
        if (m_session) fuse_session_destroy(m_session);
    }
    SessionGuard(const SessionGuard &) = delete;
    SessionGuard &operator=(const SessionGuard &) = delete;
    fuse_session *get() const { return m_session; }
    fuse_session *release() {
        auto *session = m_session;
        m_session = nullptr;
        return session;
    }

private:
    fuse_session *m_session = nullptr;
};

class ArgsGuard final {
public:
    explicit ArgsGuard(fuse_args args) : m_args(args) {}
    ~ArgsGuard() { fuse_opt_free_args(&m_args); }
    ArgsGuard(const ArgsGuard &) = delete;
    ArgsGuard &operator=(const ArgsGuard &) = delete;
    fuse_args *get() { return &m_args; }

private:
    fuse_args m_args;
};

} // namespace

class LinuxFuseMount::Impl final {
public:
    ~Impl() { stop(); }

    Result<void> start(CloudVfs::CloudVfs &vfs, LinuxFuseMountOptions options) {
        if (m_session) return make_error(ErrorCode::InvalidArgument, "FUSE mount is already running");
        if (options.mount_point.empty()) return make_error(ErrorCode::InvalidArgument, "FUSE mount point is required");

        m_vfs = &vfs;
        auto ops = fuse_lowlevel_ops{};
        ops.lookup = lookup;
        ops.getattr = getattr;
        ops.opendir = opendir;
        ops.readdir = readdir;
        ops.releasedir = releasedir;
        ops.open = open;
        ops.read = read;
        ops.mkdir = mkdir;
        ops.unlink = unlink;
        ops.rmdir = rmdir;
        ops.rename = rename;
        ops.create = create;
        ops.write = write;
        ops.flush = flush;
        ops.release = release;

        auto mount = options.mount_point.string();
        std::vector<std::string> arg_storage = {"ubaanext-fuse", "-o", "ro,default_permissions", mount};
        std::vector<char *> argv;
        argv.reserve(arg_storage.size());
        for (auto &arg : arg_storage) argv.push_back(arg.data());
        ArgsGuard args(FUSE_ARGS_INIT(static_cast<int>(argv.size()), argv.data()));
        SessionGuard session(fuse_session_new(args.get(), &ops, sizeof(ops), this));
        if (!session.get()) return make_error(ErrorCode::StorageError, "failed to create FUSE session");
        if (fuse_session_mount(session.get(), mount.c_str()) != 0) {
            return make_error(ErrorCode::StorageError, "failed to mount FUSE session");
        }

        m_mount_point = std::move(options.mount_point);
        m_session = session.release();
        if (options.foreground) {
            fuse_session_loop(m_session);
            return stop();
        }
        m_loop = std::thread([this] { fuse_session_loop(m_session); });
        return {};
    }

    Result<void> stop() {
        if (!m_session) return {};
        fuse_session_exit(m_session);
        if (m_loop.joinable()) m_loop.join();
        fuse_session_unmount(m_session);
        fuse_session_destroy(m_session);
        m_session = nullptr;
        m_vfs = nullptr;
        m_mount_point.clear();
        return {};
    }

    bool running() const { return m_session != nullptr; }

private:
    struct DirHandle {
        std::vector<CloudVfs::CloudVfsNode> entries;
    };

    static Impl *self(fuse_req_t request) {
        return static_cast<Impl *>(fuse_req_userdata(request));
    }

    std::string path_for_ino(fuse_ino_t ino) const {
        if (ino == FUSE_ROOT_ID) return "/";
        auto index = static_cast<std::size_t>(ino - 2);
        if (index >= m_paths_by_ino.size()) return {};
        return m_paths_by_ino[index];
    }

    fuse_ino_t ino_for_path(const std::string &path) {
        auto existing = std::find(m_paths_by_ino.begin(), m_paths_by_ino.end(), path);
        if (existing != m_paths_by_ino.end()) return static_cast<fuse_ino_t>((existing - m_paths_by_ino.begin()) + 2);
        m_paths_by_ino.push_back(path);
        return static_cast<fuse_ino_t>(m_paths_by_ino.size() + 1);
    }

    static void fill_attr(const CloudVfs::CloudVfsNode &node, fuse_ino_t ino, struct stat &attr) {
        std::memset(&attr, 0, sizeof(attr));
        attr.st_ino = ino;
        attr.st_mode = node.is_dir ? (S_IFDIR | 0555) : (S_IFREG | 0444);
        attr.st_nlink = node.is_dir ? 2 : 1;
        attr.st_size = static_cast<off_t>(node.size);
    }

    static void reply_entry(fuse_req_t request, Impl &impl, const CloudVfs::CloudVfsNode &node) {
        fuse_entry_param entry{};
        entry.ino = node.path.value == "/" ? FUSE_ROOT_ID : impl.ino_for_path(node.path.value);
        fill_attr(node, entry.ino, entry.attr);
        entry.attr_timeout = 1.0;
        entry.entry_timeout = 1.0;
        fuse_reply_entry(request, &entry);
    }

    static void lookup(fuse_req_t request, fuse_ino_t parent, const char *name) {
        auto &impl = *self(request);
        auto parent_path = impl.path_for_ino(parent);
        if (parent_path.empty()) return fuse_reply_err(request, ENOENT);
        auto child_path = parent_path == "/" ? "/" + std::string(name) : parent_path + "/" + std::string(name);
        auto node = impl.m_vfs->lookup(child_path);
        if (!node) return fuse_reply_err(request, error_to_errno(node.error().code));
        reply_entry(request, impl, *node);
    }

    static void getattr(fuse_req_t request, fuse_ino_t ino, fuse_file_info *) {
        auto &impl = *self(request);
        auto path = impl.path_for_ino(ino);
        if (path.empty()) return fuse_reply_err(request, ENOENT);
        auto node = impl.m_vfs->lookup(path);
        if (!node) return fuse_reply_err(request, error_to_errno(node.error().code));
        struct stat attr{};
        fill_attr(*node, ino, attr);
        fuse_reply_attr(request, &attr, 1.0);
    }

    static void opendir(fuse_req_t request, fuse_ino_t ino, fuse_file_info *file_info) {
        auto &impl = *self(request);
        auto path = impl.path_for_ino(ino);
        if (path.empty()) return fuse_reply_err(request, ENOENT);
        auto entries = impl.m_vfs->list(path);
        if (!entries) return fuse_reply_err(request, error_to_errno(entries.error().code));
        auto *handle = new DirHandle{std::move(*entries)};
        file_info->fh = reinterpret_cast<std::uint64_t>(handle);
        fuse_reply_open(request, file_info);
    }

    static void readdir(fuse_req_t request, fuse_ino_t ino, size_t size, off_t offset, fuse_file_info *file_info) {
        auto &impl = *self(request);
        auto *handle = reinterpret_cast<DirHandle *>(file_info->fh);
        if (!handle) return fuse_reply_err(request, EIO);

        std::vector<char> buffer(size);
        size_t used = 0;
        auto add = [&](const char *name, const CloudVfs::CloudVfsNode *node, off_t next_offset) -> bool {
            struct stat attr{};
            auto entry_ino = ino;
            if (node) {
                entry_ino = impl.ino_for_path(node->path.value);
                fill_attr(*node, entry_ino, attr);
            } else {
                attr.st_ino = entry_ino;
                attr.st_mode = S_IFDIR | 0555;
            }
            auto written = fuse_add_direntry(request, buffer.data() + used, buffer.size() - used, name, &attr, next_offset);
            if (written > buffer.size() - used) return false;
            used += written;
            return true;
        };

        off_t next = 1;
        if (offset < next && !add(".", nullptr, next)) return fuse_reply_buf(request, buffer.data(), used);
        ++next;
        if (offset < next && !add("..", nullptr, next)) return fuse_reply_buf(request, buffer.data(), used);
        for (std::size_t i = 0; i < handle->entries.size(); ++i) {
            ++next;
            if (offset >= next) continue;
            if (!add(handle->entries[i].name.c_str(), &handle->entries[i], next)) break;
        }
        fuse_reply_buf(request, buffer.data(), used);
    }

    static void releasedir(fuse_req_t request, fuse_ino_t, fuse_file_info *file_info) {
        delete reinterpret_cast<DirHandle *>(file_info->fh);
        file_info->fh = 0;
        fuse_reply_err(request, 0);
    }

    static void open(fuse_req_t request, fuse_ino_t ino, fuse_file_info *file_info) {
        auto &impl = *self(request);
        if ((file_info->flags & O_ACCMODE) != O_RDONLY) return fuse_reply_err(request, EROFS);
        auto path = impl.path_for_ino(ino);
        if (path.empty()) return fuse_reply_err(request, ENOENT);
        auto node = impl.m_vfs->lookup(path);
        if (!node) return fuse_reply_err(request, error_to_errno(node.error().code));
        if (node->is_dir) return fuse_reply_err(request, EISDIR);
        fuse_reply_open(request, file_info);
    }

    static void read(fuse_req_t request, fuse_ino_t ino, size_t size, off_t offset, fuse_file_info *) {
        auto &impl = *self(request);
        auto path = impl.path_for_ino(ino);
        if (path.empty()) return fuse_reply_err(request, ENOENT);
        auto node = impl.m_vfs->lookup(path);
        if (!node) return fuse_reply_err(request, error_to_errno(node.error().code));
        auto count = read_size(*node, offset, size);
        auto bytes = impl.m_vfs->read(path, static_cast<std::uint64_t>(offset), count);
        if (!bytes) return fuse_reply_err(request, error_to_errno(bytes.error().code));
        fuse_reply_buf(request, reinterpret_cast<const char *>(bytes->data()), bytes->size());
    }

    static void mkdir(fuse_req_t request, fuse_ino_t, const char *, mode_t) {
        fuse_reply_err(request, EROFS);
    }

    static void unlink(fuse_req_t request, fuse_ino_t, const char *) {
        fuse_reply_err(request, EROFS);
    }

    static void rmdir(fuse_req_t request, fuse_ino_t, const char *) {
        fuse_reply_err(request, EROFS);
    }

    static void rename(fuse_req_t request, fuse_ino_t, const char *, fuse_ino_t, const char *, unsigned int) {
        fuse_reply_err(request, EROFS);
    }

    static void create(fuse_req_t request, fuse_ino_t, const char *, mode_t, fuse_file_info *) {
        fuse_reply_err(request, EROFS);
    }

    static void write(fuse_req_t request, fuse_ino_t, const char *, size_t, off_t, fuse_file_info *) {
        fuse_reply_err(request, EROFS);
    }

    static void flush(fuse_req_t request, fuse_ino_t, fuse_file_info *) {
        fuse_reply_err(request, 0);
    }

    static void release(fuse_req_t request, fuse_ino_t, fuse_file_info *) {
        fuse_reply_err(request, 0);
    }

    CloudVfs::CloudVfs *m_vfs = nullptr;
    fuse_session *m_session = nullptr;
    std::thread m_loop;
    std::filesystem::path m_mount_point;
    std::vector<std::string> m_paths_by_ino;
};

LinuxFuseMount::LinuxFuseMount() : m_impl(std::make_unique<Impl>()) {}
LinuxFuseMount::~LinuxFuseMount() = default;

Result<void> LinuxFuseMount::start(CloudVfs::CloudVfs &vfs, LinuxFuseMountOptions options) {
    return m_impl->start(vfs, std::move(options));
}

Result<void> LinuxFuseMount::stop() {
    return m_impl->stop();
}

bool LinuxFuseMount::running() const {
    return m_impl->running();
}

} // namespace UBAANext::Platform::Linux

#else

namespace UBAANext::Platform::Linux {

class LinuxFuseMount::Impl final {
public:
    Result<void> start(CloudVfs::CloudVfs &, LinuxFuseMountOptions) {
        return make_error(ErrorCode::UnsupportedPlatform, "FUSE dependency is not available in this build/runtime");
    }
    Result<void> stop() { return {}; }
    bool running() const { return false; }
};

LinuxFuseMount::LinuxFuseMount() : m_impl(std::make_unique<Impl>()) {}
LinuxFuseMount::~LinuxFuseMount() = default;

Result<void> LinuxFuseMount::start(CloudVfs::CloudVfs &vfs, LinuxFuseMountOptions options) {
    return m_impl->start(vfs, std::move(options));
}

Result<void> LinuxFuseMount::stop() {
    return m_impl->stop();
}

bool LinuxFuseMount::running() const {
    return m_impl->running();
}

} // namespace UBAANext::Platform::Linux

#endif
