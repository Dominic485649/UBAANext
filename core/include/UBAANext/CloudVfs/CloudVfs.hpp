#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Cloud.hpp>
#include <UBAANext/Service/CloudService.hpp>
#include <UBAANext/Upload/UploadSource.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace UBAANext::CloudVfs {

enum class CloudVfsConflictPolicy {
    Fail,
    UseSuggestedName,
    Overwrite,
};

enum class CloudVfsTaskStatus {
    Pending,
    Running,
    Succeeded,
    Failed,
    Cancelled,
};

enum class CloudVfsWriteOperation {
    CreateDirectory,
    Rename,
    Move,
    Delete,
    Upload,
    ClearCache,
};

struct CloudVfsPath {
    std::string value = "/";
};

struct CloudVfsNode {
    std::string docid;
    std::string parent_id;
    std::string name;
    CloudVfsPath path;
    bool is_dir = false;
    std::uint64_t size = 0;
    std::string mtime;
    std::string rev;
    std::string token;
};

struct CloudVfsHandle {
    std::uint64_t id = 0;
    std::string docid;
    std::string rev;
    std::string path;
    bool writable = false;
};

struct CloudVfsConfig {
    std::uint64_t max_content_cache_bytes = 512ULL * 1024ULL * 1024ULL;
    int metadata_ttl_seconds = 60;
    bool read_only = true;
};

struct CloudVfsTask {
    std::uint64_t id = 0;
    CloudVfsTaskStatus status = CloudVfsTaskStatus::Pending;
    CloudVfsWriteOperation operation = CloudVfsWriteOperation::Upload;
    std::string path;
    std::string name;
    std::string parent_docid;
    std::string result_docid;
    std::string error_code;
    std::string error_message;
    int attempts = 0;
};

class ICloudVfsContentCache {
public:
    virtual ~ICloudVfsContentCache() = default;

    [[nodiscard]] virtual std::optional<std::vector<unsigned char>> get_range(const std::string &docid,
                                                                              const std::string &rev,
                                                                              std::uint64_t offset,
                                                                              std::uint64_t length) const = 0;
    virtual void put_range(const std::string &docid,
                           const std::string &rev,
                           std::uint64_t offset,
                           std::vector<unsigned char> bytes) = 0;
    virtual void remove_doc(const std::string &docid) = 0;
    virtual void trim_to(std::uint64_t max_bytes) = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual std::uint64_t used_bytes() const = 0;
};

class ICloudVfsWriteGate {
public:
    virtual ~ICloudVfsWriteGate() = default;
    [[nodiscard]] virtual Result<void> authorize(CloudVfsWriteOperation operation, const std::string &path) = 0;
};

class AllowAllCloudVfsWriteGate final : public ICloudVfsWriteGate {
public:
    [[nodiscard]] Result<void> authorize(CloudVfsWriteOperation operation, const std::string &path) override;
};

class ReadOnlyCloudVfsWriteGate final : public ICloudVfsWriteGate {
public:
    [[nodiscard]] Result<void> authorize(CloudVfsWriteOperation operation, const std::string &path) override;
};

class MemoryCloudVfsContentCache final : public ICloudVfsContentCache {
public:
    [[nodiscard]] std::optional<std::vector<unsigned char>> get_range(const std::string &docid,
                                                                      const std::string &rev,
                                                                      std::uint64_t offset,
                                                                      std::uint64_t length) const override;
    void put_range(const std::string &docid,
                   const std::string &rev,
                   std::uint64_t offset,
                   std::vector<unsigned char> bytes) override;
    void remove_doc(const std::string &docid) override;
    void trim_to(std::uint64_t max_bytes) override;
    void clear() override;
    [[nodiscard]] std::uint64_t used_bytes() const override;

private:
    struct RangeEntry {
        std::string docid;
        std::string rev;
        std::uint64_t offset = 0;
        std::vector<unsigned char> bytes;
        std::uint64_t sequence = 0;
    };

    std::vector<RangeEntry> m_entries;
    std::uint64_t m_used_bytes = 0;
    std::uint64_t m_sequence = 0;
};

class CloudVfs final {
public:
    CloudVfs(CloudService &service, ICloudVfsContentCache &content_cache, CloudVfsConfig config = {});

    void set_write_gate(ICloudVfsWriteGate *gate);
    [[nodiscard]] Result<CloudVfsNode> load_user_root();
    [[nodiscard]] Result<void> set_root(const Model::CloudItem &item);
    [[nodiscard]] Result<CloudVfsNode> root() const;
    [[nodiscard]] Result<CloudVfsNode> lookup(const std::string &path) const;
    [[nodiscard]] Result<std::vector<CloudVfsNode>> list(const std::string &path, bool refresh = false);
    [[nodiscard]] Result<std::vector<CloudVfsNode>> refresh(const std::string &path);
    [[nodiscard]] Result<std::vector<unsigned char>> read(const std::string &path, std::uint64_t offset, std::uint64_t length);
    [[nodiscard]] Result<CloudVfsTask> create_directory(const std::string &parent_path, std::string name);
    [[nodiscard]] Result<CloudVfsTask> rename(const std::string &path, std::string new_name);
    [[nodiscard]] Result<CloudVfsTask> remove(const std::string &path);
    [[nodiscard]] Result<CloudVfsTask> enqueue_upload(const std::string &parent_path,
                                                      std::string name,
                                                      std::shared_ptr<IUploadSource> source,
                                                      CloudVfsConflictPolicy conflict_policy);
    [[nodiscard]] Result<CloudVfsHandle> open_temp_write(const std::string &parent_path,
                                                         std::string name,
                                                         CloudVfsConflictPolicy conflict_policy);
    [[nodiscard]] Result<void> write(CloudVfsHandle &handle, std::uint64_t offset, const std::vector<unsigned char> &bytes);
    [[nodiscard]] Result<CloudVfsTask> flush(CloudVfsHandle &handle);
    [[nodiscard]] Result<CloudVfsTask> close(CloudVfsHandle &handle);
    [[nodiscard]] Result<void> discard(CloudVfsHandle &handle);
    [[nodiscard]] Result<CloudVfsTask> process_next_upload();
    [[nodiscard]] Result<CloudVfsTask> retry_upload(std::uint64_t task_id);
    [[nodiscard]] Result<CloudVfsTask> cancel_upload(std::uint64_t task_id);
    std::size_t cleanup_tasks();
    [[nodiscard]] std::vector<CloudVfsTask> tasks() const;
    [[nodiscard]] Result<void> clear_content_cache();

private:
    struct MetadataEntry {
        std::vector<std::string> child_docids;
        std::chrono::steady_clock::time_point loaded_at{};
        bool loaded = false;
    };

    struct PendingUpload {
        CloudVfsTask task;
        std::shared_ptr<IUploadSource> source;
        std::string final_name;
        std::string token;
        CloudVfsConflictPolicy conflict_policy = CloudVfsConflictPolicy::Fail;
    };

    struct PendingWrite {
        CloudVfsHandle handle;
        std::string parent_path;
        std::string parent_docid;
        std::string name;
        std::string token;
        CloudVfsConflictPolicy conflict_policy = CloudVfsConflictPolicy::Fail;
        std::vector<unsigned char> bytes;
        std::uint64_t flushed_task_id = 0;
        bool flushed = false;
    };

    [[nodiscard]] Result<std::string> normalize_path_result(const std::string &path) const;
    [[nodiscard]] Result<void> authorize(CloudVfsWriteOperation operation, const std::string &path) const;
    [[nodiscard]] Result<std::string> resolve_upload_name(const CloudVfsNode &parent,
                                                          const std::string &name,
                                                          CloudVfsConflictPolicy policy);
    [[nodiscard]] Result<void> apply_overwrite_conflict(const CloudVfsNode &parent, const std::string &name);
    [[nodiscard]] Result<PendingWrite *> pending_write(CloudVfsHandle &handle);
    void invalidate_parent(const std::string &parent_docid);
    void add_node(const CloudVfsNode &node);
    void remove_subtree(const std::string &docid);
    void cache_children(const CloudVfsNode &parent, const std::vector<Model::CloudItem> &items);
    [[nodiscard]] bool metadata_fresh(const std::string &docid) const;
    [[nodiscard]] std::string child_path(const CloudVfsNode &parent, const std::string &name) const;

    CloudService &m_service;
    ICloudVfsContentCache &m_content_cache;
    CloudVfsConfig m_config;
    ICloudVfsWriteGate *m_write_gate = nullptr;
    std::unordered_map<std::string, CloudVfsNode> m_nodes_by_docid;
    std::unordered_map<std::string, std::string> m_docid_by_path;
    std::unordered_map<std::string, MetadataEntry> m_metadata;
    std::vector<PendingUpload> m_uploads;
    std::vector<CloudVfsTask> m_tasks;
    std::unordered_map<std::uint64_t, PendingWrite> m_writes;
    std::uint64_t m_next_task_id = 1;
    std::uint64_t m_next_handle_id = 1;
    std::string m_root_docid;
};

} // namespace UBAANext::CloudVfs
