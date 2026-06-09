#include <UBAANext/CloudVfs/CloudVfs.hpp>

#include <UBAANext/Security/SecurityRedaction.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>

namespace UBAANext::CloudVfs {
namespace {

std::string operation_text(CloudVfsWriteOperation operation) {
    switch (operation) {
    case CloudVfsWriteOperation::CreateDirectory: return "mkdir";
    case CloudVfsWriteOperation::Rename: return "rename";
    case CloudVfsWriteOperation::Move: return "move";
    case CloudVfsWriteOperation::Delete: return "delete";
    case CloudVfsWriteOperation::Upload: return "upload";
    case CloudVfsWriteOperation::ClearCache: return "clear-cache";
    }
    return "write";
}

std::string error_code_text(ErrorCode code) {
    return std::string(error_code_to_string(code));
}

std::uint64_t parse_size(const std::string &value, bool is_dir) {
    if (is_dir || value.empty() || value == "-1") return 0;
    try {
        return static_cast<std::uint64_t>(std::stoull(value));
    } catch (...) {
        return 0;
    }
}

std::string sanitize_name(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
    return value;
}

Error sanitize_error(const Error &error) {
    return Error{error.code, Security::redact_sensitive_text(error.message)};
}

CloudVfsNode item_to_node(const Model::CloudItem &item, const CloudVfsNode &parent, std::string path) {
    CloudVfsNode node;
    node.docid = item.id;
    node.parent_id = item.parent_id.empty() ? parent.docid : item.parent_id;
    node.name = item.name;
    node.path.value = std::move(path);
    node.is_dir = item.is_dir();
    node.size = parse_size(item.size, node.is_dir);
    node.mtime = item.updated_at;
    node.rev = item.revision;
    node.token = item.token.empty() ? parent.token : item.token;
    return node;
}

class MemoryUploadSource final : public IUploadSource {
public:
    MemoryUploadSource(std::string name, std::vector<unsigned char> bytes)
        : m_name(std::move(name)), m_bytes(std::move(bytes)) {}

    std::string name() const override { return m_name; }
    std::string content_type() const override { return "application/octet-stream"; }
    Result<std::uint64_t> size() override { return static_cast<std::uint64_t>(m_bytes.size()); }
    Result<void> rewind() override {
        m_offset = 0;
        return {};
    }
    Result<std::size_t> read(unsigned char *buffer, std::size_t max_bytes) override {
        const auto remaining = m_bytes.size() - m_offset;
        const auto count = std::min(max_bytes, remaining);
        std::copy_n(m_bytes.data() + m_offset, count, buffer);
        m_offset += count;
        return count;
    }

private:
    std::string m_name;
    std::vector<unsigned char> m_bytes;
    std::size_t m_offset = 0;
};

} // namespace

Result<void> AllowAllCloudVfsWriteGate::authorize(CloudVfsWriteOperation operation, const std::string &path) {
    (void)operation;
    (void)path;
    return {};
}

Result<void> ReadOnlyCloudVfsWriteGate::authorize(CloudVfsWriteOperation operation, const std::string &path) {
    (void)operation;
    return make_error(ErrorCode::InvalidArgument, "Cloud VFS 写操作未授权: " + Security::redact_sensitive_text(path));
}

std::optional<std::vector<unsigned char>> MemoryCloudVfsContentCache::get_range(const std::string &docid,
                                                                                const std::string &rev,
                                                                                std::uint64_t offset,
                                                                                std::uint64_t length) const {
    for (const auto &entry : m_entries) {
        if (entry.docid != docid || entry.rev != rev) continue;
        if (offset < entry.offset) continue;
        const auto relative = offset - entry.offset;
        if (relative > entry.bytes.size()) continue;
        if (length > entry.bytes.size() - relative) continue;
        auto first = entry.bytes.begin() + static_cast<std::ptrdiff_t>(relative);
        auto last = first + static_cast<std::ptrdiff_t>(length);
        return std::vector<unsigned char>(first, last);
    }
    return std::nullopt;
}

void MemoryCloudVfsContentCache::put_range(const std::string &docid,
                                           const std::string &rev,
                                           std::uint64_t offset,
                                           std::vector<unsigned char> bytes) {
    m_used_bytes += static_cast<std::uint64_t>(bytes.size());
    m_entries.push_back(RangeEntry{docid, rev, offset, std::move(bytes), ++m_sequence});
}

void MemoryCloudVfsContentCache::remove_doc(const std::string &docid) {
    auto it = std::remove_if(m_entries.begin(), m_entries.end(), [&](const RangeEntry &entry) {
        if (entry.docid != docid) return false;
        m_used_bytes -= static_cast<std::uint64_t>(entry.bytes.size());
        return true;
    });
    m_entries.erase(it, m_entries.end());
}

void MemoryCloudVfsContentCache::trim_to(std::uint64_t max_bytes) {
    while (m_used_bytes > max_bytes && !m_entries.empty()) {
        auto oldest = std::min_element(m_entries.begin(), m_entries.end(), [](const RangeEntry &left, const RangeEntry &right) {
            return left.sequence < right.sequence;
        });
        m_used_bytes -= static_cast<std::uint64_t>(oldest->bytes.size());
        m_entries.erase(oldest);
    }
}

void MemoryCloudVfsContentCache::clear() {
    m_entries.clear();
    m_used_bytes = 0;
}

std::uint64_t MemoryCloudVfsContentCache::used_bytes() const {
    return m_used_bytes;
}

CloudVfs::CloudVfs(CloudService &service, ICloudVfsContentCache &content_cache, CloudVfsConfig config)
    : m_service(service), m_content_cache(content_cache), m_config(std::move(config)) {}

void CloudVfs::set_write_gate(ICloudVfsWriteGate *gate) {
    m_write_gate = gate;
}

Result<CloudVfsNode> CloudVfs::load_user_root() {
    auto root_item = m_service.user_root();
    if (!root_item) return make_error(root_item.error().code, root_item.error().message);
    auto set = set_root(*root_item);
    if (!set) return make_error(set.error().code, set.error().message);
    return root();
}

Result<void> CloudVfs::set_root(const Model::CloudItem &item) {
    if (item.id.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS root 缺少 docid");
    m_nodes_by_docid.clear();
    m_docid_by_path.clear();
    m_metadata.clear();

    CloudVfsNode node;
    node.docid = item.id;
    node.name = item.name.empty() ? "cloud" : item.name;
    node.path.value = "/";
    node.is_dir = true;
    node.mtime = item.updated_at;
    node.rev = item.revision;
    node.token = item.token;
    m_root_docid = node.docid;
    add_node(node);
    return {};
}

Result<CloudVfsNode> CloudVfs::root() const {
    if (m_root_docid.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS root 尚未加载");
    auto it = m_nodes_by_docid.find(m_root_docid);
    if (it == m_nodes_by_docid.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS root 尚未加载");
    return it->second;
}

Result<std::string> CloudVfs::normalize_path_result(const std::string &path) const {
    if (path.empty()) return std::string{"/"};
    std::string text = path;
    std::replace(text.begin(), text.end(), '\\', '/');

    std::vector<std::string> parts;
    std::istringstream input(text);
    std::string part;
    while (std::getline(input, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") return make_error(ErrorCode::InvalidArgument, "Cloud VFS 路径不能包含 ..");
        parts.push_back(part);
    }

    std::string out = "/";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += "/";
        out += parts[i];
    }
    return out;
}

Result<CloudVfsNode> CloudVfs::lookup(const std::string &path) const {
    auto normalized = normalize_path_result(path);
    if (!normalized) return make_error(normalized.error().code, normalized.error().message);
    auto path_it = m_docid_by_path.find(*normalized);
    if (path_it == m_docid_by_path.end()) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS 路径未加载或不存在: " + Security::redact_sensitive_text(*normalized));
    }
    auto node_it = m_nodes_by_docid.find(path_it->second);
    if (node_it == m_nodes_by_docid.end()) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS 元数据索引损坏");
    }
    return node_it->second;
}

Result<std::vector<CloudVfsNode>> CloudVfs::list(const std::string &path, bool refresh) {
    auto parent = lookup(path);
    if (!parent) return make_error(parent.error().code, parent.error().message);
    if (!parent->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 只能列出目录");

    if (!refresh && metadata_fresh(parent->docid)) {
        std::vector<CloudVfsNode> cached;
        for (const auto &docid : m_metadata[parent->docid].child_docids) {
            auto it = m_nodes_by_docid.find(docid);
            if (it != m_nodes_by_docid.end()) cached.push_back(it->second);
        }
        return cached;
    }

    CloudListQuery query;
    query.doc_id = parent->docid;
    query.token = parent->token;
    auto dir = m_service.list_dir(query);
    if (!dir) return make_error(dir.error().code, dir.error().message);

    std::vector<Model::CloudItem> items;
    items.reserve(dir->dirs.size() + dir->files.size());
    items.insert(items.end(), dir->dirs.begin(), dir->dirs.end());
    items.insert(items.end(), dir->files.begin(), dir->files.end());
    cache_children(*parent, items);

    std::vector<CloudVfsNode> children;
    for (const auto &docid : m_metadata[parent->docid].child_docids) {
        auto it = m_nodes_by_docid.find(docid);
        if (it != m_nodes_by_docid.end()) children.push_back(it->second);
    }
    return children;
}

Result<std::vector<CloudVfsNode>> CloudVfs::refresh(const std::string &path) {
    return list(path, true);
}

Result<std::vector<unsigned char>> CloudVfs::read(const std::string &path, std::uint64_t offset, std::uint64_t length) {
    if (length == 0) return std::vector<unsigned char>{};
    auto node = lookup(path);
    if (!node) return make_error(node.error().code, node.error().message);
    if (node->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 不能读取目录内容");
    if (offset >= node->size) return std::vector<unsigned char>{};
    length = std::min(length, node->size - offset);

    auto cached = m_content_cache.get_range(node->docid, node->rev, offset, length);
    if (cached) return *cached;

    CloudItemRef item;
    item.doc_id = node->docid;
    item.name = node->name;
    item.is_dir = false;
    item.token = node->token;
    auto chunk = m_service.download_range(item, offset, length);
    if (!chunk) {
        auto error = sanitize_error(chunk.error());
        return make_error(error.code, error.message);
    }
    if (chunk->bytes.size() > length) chunk->bytes.resize(static_cast<std::size_t>(length));
    m_content_cache.put_range(node->docid, node->rev, offset, chunk->bytes);
    m_content_cache.trim_to(m_config.max_content_cache_bytes);
    return chunk->bytes;
}

Result<CloudVfsTask> CloudVfs::create_directory(const std::string &parent_path, std::string name) {
    auto parent = lookup(parent_path);
    if (!parent) return make_error(parent.error().code, parent.error().message);
    if (!parent->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud VFS mkdir 目标不是目录");
    auto allowed = authorize(CloudVfsWriteOperation::CreateDirectory, parent->path.value);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    name = sanitize_name(std::move(name));
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS mkdir 名称为空");

    CloudVfsTask task;
    task.id = m_next_task_id++;
    task.operation = CloudVfsWriteOperation::CreateDirectory;
    task.status = CloudVfsTaskStatus::Running;
    task.path = child_path(*parent, name);
    task.name = name;
    task.parent_docid = parent->docid;
    task.attempts = 1;

    auto result = m_service.create_dir(parent->docid, name);
    if (!result) {
        auto error = sanitize_error(result.error());
        task.status = CloudVfsTaskStatus::Failed;
        task.error_code = error_code_text(error.code);
        task.error_message = error.message;
        m_tasks.push_back(task);
        return make_error(error.code, error.message);
    }

    task.status = CloudVfsTaskStatus::Succeeded;
    task.result_docid = result->summary.id;
    invalidate_parent(parent->docid);
    m_tasks.push_back(task);
    return task;
}

Result<CloudVfsTask> CloudVfs::rename(const std::string &path, std::string new_name) {
    auto node = lookup(path);
    if (!node) return make_error(node.error().code, node.error().message);
    auto allowed = authorize(CloudVfsWriteOperation::Rename, node->path.value);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    new_name = sanitize_name(std::move(new_name));
    if (new_name.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS rename 名称为空");

    CloudVfsTask task;
    task.id = m_next_task_id++;
    task.operation = CloudVfsWriteOperation::Rename;
    task.status = CloudVfsTaskStatus::Running;
    task.path = node->path.value;
    task.name = new_name;
    task.parent_docid = node->parent_id;
    task.attempts = 1;

    auto result = m_service.rename_item(node->docid, new_name);
    if (!result) {
        auto error = sanitize_error(result.error());
        task.status = CloudVfsTaskStatus::Failed;
        task.error_code = error_code_text(error.code);
        task.error_message = error.message;
        m_tasks.push_back(task);
        return make_error(error.code, error.message);
    }

    task.status = CloudVfsTaskStatus::Succeeded;
    task.result_docid = node->docid;
    invalidate_parent(node->parent_id);
    remove_subtree(node->docid);
    m_tasks.push_back(task);
    return task;
}

Result<CloudVfsTask> CloudVfs::remove(const std::string &path) {
    auto node = lookup(path);
    if (!node) return make_error(node.error().code, node.error().message);
    auto allowed = authorize(CloudVfsWriteOperation::Delete, node->path.value);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    CloudVfsTask task;
    task.id = m_next_task_id++;
    task.operation = CloudVfsWriteOperation::Delete;
    task.status = CloudVfsTaskStatus::Running;
    task.path = node->path.value;
    task.name = node->name;
    task.parent_docid = node->parent_id;
    task.attempts = 1;

    auto result = m_service.delete_item(node->docid);
    if (!result) {
        auto error = sanitize_error(result.error());
        task.status = CloudVfsTaskStatus::Failed;
        task.error_code = error_code_text(error.code);
        task.error_message = error.message;
        m_tasks.push_back(task);
        return make_error(error.code, error.message);
    }

    task.status = CloudVfsTaskStatus::Succeeded;
    task.result_docid = node->docid;
    invalidate_parent(node->parent_id);
    remove_subtree(node->docid);
    m_tasks.push_back(task);
    return task;
}

Result<CloudVfsTask> CloudVfs::enqueue_upload(const std::string &parent_path,
                                              std::string name,
                                              std::shared_ptr<IUploadSource> source,
                                              CloudVfsConflictPolicy conflict_policy) {
    if (!source) return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload 缺少字节源");
    auto parent = lookup(parent_path);
    if (!parent) return make_error(parent.error().code, parent.error().message);
    if (!parent->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload 目标不是目录");
    auto allowed = authorize(CloudVfsWriteOperation::Upload, parent->path.value);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    if (name.empty()) name = source->name();
    name = sanitize_name(std::move(name));
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload 文件名为空");

    auto final_name = resolve_upload_name(*parent, name, conflict_policy);
    if (!final_name) return make_error(final_name.error().code, final_name.error().message);

    PendingUpload upload;
    upload.task.id = m_next_task_id++;
    upload.task.operation = CloudVfsWriteOperation::Upload;
    upload.task.status = CloudVfsTaskStatus::Pending;
    upload.task.path = child_path(*parent, *final_name);
    upload.task.name = *final_name;
    upload.task.parent_docid = parent->docid;
    upload.source = std::move(source);
    upload.final_name = *final_name;
    upload.token = parent->token;
    upload.conflict_policy = conflict_policy;
    m_uploads.push_back(std::move(upload));
    return m_uploads.back().task;
}

Result<CloudVfsHandle> CloudVfs::open_temp_write(const std::string &parent_path,
                                                 std::string name,
                                                 CloudVfsConflictPolicy conflict_policy) {
    auto parent = lookup(parent_path);
    if (!parent) return make_error(parent.error().code, parent.error().message);
    if (!parent->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write 目标不是目录");
    auto allowed = authorize(CloudVfsWriteOperation::Upload, parent->path.value);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    name = sanitize_name(std::move(name));
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write 文件名为空");
    auto final_name = resolve_upload_name(*parent, name, conflict_policy);
    if (!final_name) return make_error(final_name.error().code, final_name.error().message);

    PendingWrite write;
    write.handle.id = m_next_handle_id++;
    write.handle.path = child_path(*parent, *final_name);
    write.handle.writable = true;
    write.parent_path = parent->path.value;
    write.parent_docid = parent->docid;
    write.name = *final_name;
    write.token = parent->token;
    write.conflict_policy = conflict_policy;
    auto handle = write.handle;
    m_writes.emplace(handle.id, std::move(write));
    return handle;
}

Result<void> CloudVfs::write(CloudVfsHandle &handle, std::uint64_t offset, const std::vector<unsigned char> &bytes) {
    auto pending = pending_write(handle);
    if (!pending) return make_error(pending.error().code, pending.error().message);
    if ((*pending)->flushed) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write 已经 flush");
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS write offset 超出内存缓存上限");
    }
    if (bytes.size() > std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(offset)) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS write 大小超出内存缓存上限");
    }
    if (offset > (*pending)->bytes.size()) (*pending)->bytes.resize(static_cast<std::size_t>(offset));
    const auto required = static_cast<std::size_t>(offset) + bytes.size();
    if (required > (*pending)->bytes.size()) (*pending)->bytes.resize(required);
    std::copy(bytes.begin(), bytes.end(), (*pending)->bytes.begin() + static_cast<std::ptrdiff_t>(offset));
    return {};
}

Result<CloudVfsTask> CloudVfs::flush(CloudVfsHandle &handle) {
    auto pending = pending_write(handle);
    if (!pending) return make_error(pending.error().code, pending.error().message);
    if ((*pending)->flushed) {
        auto task_it = std::find_if(m_uploads.begin(), m_uploads.end(), [&](const PendingUpload &upload) {
            return upload.task.id == (*pending)->flushed_task_id;
        });
        if (task_it == m_uploads.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write task 已清理");
        return task_it->task;
    }

    PendingUpload upload;
    upload.task.id = m_next_task_id++;
    upload.task.operation = CloudVfsWriteOperation::Upload;
    upload.task.status = CloudVfsTaskStatus::Pending;
    upload.task.path = (*pending)->handle.path;
    upload.task.name = (*pending)->name;
    upload.task.parent_docid = (*pending)->parent_docid;
    upload.source = std::make_shared<MemoryUploadSource>((*pending)->name, (*pending)->bytes);
    upload.final_name = (*pending)->name;
    upload.token = (*pending)->token;
    upload.conflict_policy = (*pending)->conflict_policy;
    m_uploads.push_back(std::move(upload));

    (*pending)->flushed = true;
    (*pending)->flushed_task_id = m_uploads.back().task.id;
    return m_uploads.back().task;
}

Result<CloudVfsTask> CloudVfs::close(CloudVfsHandle &handle) {
    auto pending = pending_write(handle);
    if (!pending) return make_error(pending.error().code, pending.error().message);
    auto flushed = flush(handle);
    if (!flushed) return make_error(flushed.error().code, flushed.error().message);
    m_writes.erase(handle.id);
    handle.writable = false;
    return *flushed;
}

Result<void> CloudVfs::discard(CloudVfsHandle &handle) {
    auto pending = pending_write(handle);
    if (!pending) return make_error(pending.error().code, pending.error().message);
    if ((*pending)->flushed) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write 已经 flush，不能丢弃");
    m_writes.erase(handle.id);
    handle.writable = false;
    return {};
}

Result<CloudVfsTask> CloudVfs::process_next_upload() {
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(), [](const PendingUpload &upload) {
        return upload.task.status == CloudVfsTaskStatus::Pending;
    });
    if (it == m_uploads.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 没有待处理上传任务");

    it->task.status = CloudVfsTaskStatus::Running;
    ++it->task.attempts;
    it->task.error_code.clear();
    it->task.error_message.clear();

    auto parent_it = m_nodes_by_docid.find(it->task.parent_docid);
    if (parent_it == m_nodes_by_docid.end()) {
        it->task.status = CloudVfsTaskStatus::Failed;
        it->task.error_code = error_code_text(ErrorCode::InvalidArgument);
        it->task.error_message = "Cloud VFS upload 父目录未加载";
        return it->task;
    }

    if (it->conflict_policy == CloudVfsConflictPolicy::Overwrite) {
        auto overwritten = apply_overwrite_conflict(parent_it->second, it->final_name);
        if (!overwritten) {
            auto error = sanitize_error(overwritten.error());
            it->task.status = CloudVfsTaskStatus::Failed;
            it->task.error_code = error_code_text(error.code);
            it->task.error_message = error.message;
            return it->task;
        }
    }

    CloudUploadRequest request;
    request.parent_id = it->task.parent_docid;
    request.name = it->final_name;
    request.token = it->token;
    auto result = m_service.upload_file(request, *it->source);
    if (!result) {
        auto error = sanitize_error(result.error());
        it->task.status = CloudVfsTaskStatus::Failed;
        it->task.error_code = error_code_text(error.code);
        it->task.error_message = error.message;
        return it->task;
    }

    it->task.status = CloudVfsTaskStatus::Succeeded;
    it->task.result_docid = result->summary.id;
    it->task.error_code.clear();
    it->task.error_message.clear();
    invalidate_parent(it->task.parent_docid);
    m_content_cache.remove_doc(it->task.result_docid);
    auto parent_it_after_upload = m_nodes_by_docid.find(it->task.parent_docid);
    if (parent_it_after_upload != m_nodes_by_docid.end()) {
        const auto uploaded_path = child_path(parent_it_after_upload->second, it->final_name);
        auto uploaded_path_it = m_docid_by_path.find(uploaded_path);
        if (uploaded_path_it != m_docid_by_path.end()) m_content_cache.remove_doc(uploaded_path_it->second);
    }
    return it->task;
}

Result<CloudVfsTask> CloudVfs::retry_upload(std::uint64_t task_id) {
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(), [&](const PendingUpload &upload) {
        return upload.task.id == task_id;
    });
    if (it == m_uploads.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload task 不存在");
    if (it->task.status != CloudVfsTaskStatus::Failed) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 只能重试失败任务");
    it->task.status = CloudVfsTaskStatus::Pending;
    it->task.error_code.clear();
    it->task.error_message.clear();
    return it->task;
}

Result<CloudVfsTask> CloudVfs::cancel_upload(std::uint64_t task_id) {
    auto it = std::find_if(m_uploads.begin(), m_uploads.end(), [&](const PendingUpload &upload) {
        return upload.task.id == task_id;
    });
    if (it == m_uploads.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload task 不存在");
    if (it->task.status == CloudVfsTaskStatus::Running) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 不能取消运行中任务");
    if (it->task.status == CloudVfsTaskStatus::Succeeded) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 不能取消已成功任务");
    if (it->task.status == CloudVfsTaskStatus::Cancelled) return it->task;
    it->task.status = CloudVfsTaskStatus::Cancelled;
    it->task.error_code.clear();
    it->task.error_message.clear();
    return it->task;
}

std::size_t CloudVfs::cleanup_tasks() {
    const auto upload_before = m_uploads.size();
    auto upload_it = std::remove_if(m_uploads.begin(), m_uploads.end(), [](const PendingUpload &upload) {
        return upload.task.status == CloudVfsTaskStatus::Succeeded || upload.task.status == CloudVfsTaskStatus::Cancelled;
    });
    m_uploads.erase(upload_it, m_uploads.end());

    const auto task_before = m_tasks.size();
    auto task_it = std::remove_if(m_tasks.begin(), m_tasks.end(), [](const CloudVfsTask &task) {
        return task.status == CloudVfsTaskStatus::Succeeded || task.status == CloudVfsTaskStatus::Cancelled;
    });
    m_tasks.erase(task_it, m_tasks.end());
    return (upload_before - m_uploads.size()) + (task_before - m_tasks.size());
}

std::vector<CloudVfsTask> CloudVfs::tasks() const {
    std::vector<CloudVfsTask> out;
    out.reserve(m_uploads.size() + m_tasks.size());
    for (const auto &task : m_tasks) out.push_back(task);
    for (const auto &upload : m_uploads) out.push_back(upload.task);
    return out;
}

Result<void> CloudVfs::clear_content_cache() {
    auto allowed = authorize(CloudVfsWriteOperation::ClearCache, "/");
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    m_content_cache.clear();
    return {};
}

Result<void> CloudVfs::authorize(CloudVfsWriteOperation operation, const std::string &path) const {
    if (m_config.read_only && operation != CloudVfsWriteOperation::ClearCache) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS 只读模式拒绝 " + operation_text(operation));
    }
    if (!m_write_gate) return make_error(ErrorCode::InvalidArgument, "Cloud VFS 写操作未授权: " + operation_text(operation));
    auto result = m_write_gate->authorize(operation, path);
    if (!result) return make_error(result.error().code, result.error().message);
    return {};
}

Result<std::string> CloudVfs::resolve_upload_name(const CloudVfsNode &parent,
                                                  const std::string &name,
                                                  CloudVfsConflictPolicy policy) {
    auto children = list(parent.path.value, false);
    if (!children) return make_error(children.error().code, children.error().message);
    const auto exists = std::any_of(children->begin(), children->end(), [&](const CloudVfsNode &child) {
        return child.name == name;
    });
    if (!exists || policy == CloudVfsConflictPolicy::Overwrite) return name;
    if (policy == CloudVfsConflictPolicy::Fail) {
        return make_error(ErrorCode::InvalidArgument, "Cloud VFS upload 存在同名条目");
    }

    auto suggested = m_service.suggest_name(parent.docid, name);
    if (!suggested) return make_error(suggested.error().code, suggested.error().message);
    return *suggested;
}

Result<void> CloudVfs::apply_overwrite_conflict(const CloudVfsNode &parent, const std::string &name) {
    auto children = refresh(parent.path.value);
    if (!children) return make_error(children.error().code, children.error().message);
    for (const auto &child : *children) {
        if (child.name != name) continue;
        auto allowed = authorize(CloudVfsWriteOperation::Delete, child.path.value);
        if (!allowed) return make_error(allowed.error().code, allowed.error().message);
        auto deleted = m_service.delete_item(child.docid);
        if (!deleted) return make_error(deleted.error().code, deleted.error().message);
        remove_subtree(child.docid);
        invalidate_parent(parent.docid);
        return {};
    }
    return {};
}

Result<CloudVfs::PendingWrite *> CloudVfs::pending_write(CloudVfsHandle &handle) {
    if (!handle.writable || handle.id == 0) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write handle 无效");
    auto it = m_writes.find(handle.id);
    if (it == m_writes.end()) return make_error(ErrorCode::InvalidArgument, "Cloud VFS write handle 不存在");
    return &it->second;
}

void CloudVfs::invalidate_parent(const std::string &parent_docid) {
    auto metadata_it = m_metadata.find(parent_docid);
    if (metadata_it != m_metadata.end()) metadata_it->second.loaded = false;
}

void CloudVfs::add_node(const CloudVfsNode &node) {
    auto existing = m_nodes_by_docid.find(node.docid);
    if (existing != m_nodes_by_docid.end() && existing->second.path.value != node.path.value) {
        m_docid_by_path.erase(existing->second.path.value);
    }
    m_nodes_by_docid[node.docid] = node;
    m_docid_by_path[node.path.value] = node.docid;
}

void CloudVfs::remove_subtree(const std::string &docid) {
    auto children_it = m_metadata.find(docid);
    if (children_it != m_metadata.end()) {
        auto children = children_it->second.child_docids;
        for (const auto &child : children) remove_subtree(child);
        m_metadata.erase(children_it);
    }
    auto node_it = m_nodes_by_docid.find(docid);
    if (node_it != m_nodes_by_docid.end()) {
        m_docid_by_path.erase(node_it->second.path.value);
        m_nodes_by_docid.erase(node_it);
    }
    m_content_cache.remove_doc(docid);
}

void CloudVfs::cache_children(const CloudVfsNode &parent, const std::vector<Model::CloudItem> &items) {
    auto existing = m_metadata[parent.docid].child_docids;
    for (const auto &docid : existing) remove_subtree(docid);

    MetadataEntry entry;
    entry.loaded = true;
    entry.loaded_at = std::chrono::steady_clock::now();
    for (const auto &item : items) {
        if (item.id.empty()) continue;
        auto node = item_to_node(item, parent, child_path(parent, item.name));
        entry.child_docids.push_back(node.docid);
        add_node(node);
    }
    m_metadata[parent.docid] = std::move(entry);
}

bool CloudVfs::metadata_fresh(const std::string &docid) const {
    auto it = m_metadata.find(docid);
    if (it == m_metadata.end() || !it->second.loaded) return false;
    if (m_config.metadata_ttl_seconds <= 0) return true;
    const auto age = std::chrono::steady_clock::now() - it->second.loaded_at;
    return age < std::chrono::seconds(m_config.metadata_ttl_seconds);
}

std::string CloudVfs::child_path(const CloudVfsNode &parent, const std::string &name) const {
    auto clean = sanitize_name(name);
    if (parent.path.value == "/") return "/" + clean;
    return parent.path.value + "/" + clean;
}

} // namespace UBAANext::CloudVfs
