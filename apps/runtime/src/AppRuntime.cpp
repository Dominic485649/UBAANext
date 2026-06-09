#include <UBAANext/Runtime/AppRuntime.hpp>

#include <UBAANext/Runtime/CloudMountAdapter.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Version.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <type_traits>
#include <system_error>
#include <utility>

namespace UBAANext::Runtime {
namespace {

std::filesystem::path env_path(const char *name) {
#if defined(_WIN32)
    char *value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr || length == 0) return {};
    std::string text(value);
    std::free(value);
    if (text.empty()) return {};
    return std::filesystem::path(text);
#else
    const auto *value = std::getenv(name);
    if (value == nullptr || *value == '\0') return {};
    return std::filesystem::path(value);
#endif
}

Result<std::string> normalize_cloud_browser_path(std::string path) {
    if (path.empty()) return std::string{"/"};
    std::replace(path.begin(), path.end(), '\\', '/');

    std::vector<std::string> parts;
    std::istringstream input(path);
    std::string part;
    while (std::getline(input, part, '/')) {
        if (part.empty() || part == ".") continue;
        if (part == "..") return make_error(ErrorCode::InvalidArgument, "Cloud path cannot contain ..");
        parts.push_back(std::move(part));
    }

    std::string normalized = "/";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) normalized += "/";
        normalized += parts[i];
    }
    return normalized;
}

std::vector<std::string> cloud_browser_path_parts(const std::string &normalized_path) {
    std::vector<std::string> parts;
    std::istringstream input(normalized_path);
    std::string part;
    while (std::getline(input, part, '/')) {
        if (!part.empty()) parts.push_back(std::move(part));
    }
    return parts;
}

Result<CloudVfs::CloudVfsNode> find_child_dir(CloudVfs::CloudVfs &vfs, const std::string &parent_path, const std::string &name) {
    auto children = vfs.list(parent_path);
    if (!children) return make_error(children.error().code, children.error().message);
    auto child = std::find_if(children->begin(), children->end(), [&](const CloudVfs::CloudVfsNode &node) {
        return node.name == name;
    });
    if (child == children->end()) return make_error(ErrorCode::InvalidArgument, "Cloud path segment not found: " + Security::redact_sensitive_text(name));
    if (!child->is_dir) return make_error(ErrorCode::InvalidArgument, "Cloud path segment is not a directory: " + Security::redact_sensitive_text(name));
    return *child;
}

std::string mode_text(ConnectionMode mode) {
    switch (mode) {
    case ConnectionMode::Direct: return "direct";
    case ConnectionMode::WebVPN: return "vpn";
    case ConnectionMode::Mock: return "mock";
    }
    return "unknown";
}

std::string redact_path(const std::filesystem::path &path) {
    if (path.empty()) return {};
    return Security::redact_sensitive_text(path.string());
}

std::uintmax_t directory_size(const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return 0;
    std::uintmax_t total = 0;
    for (std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_regular_file(ec)) total += it->file_size(ec);
    }
    return total;
}

std::string account_summary(const Model::Account &account) {
    auto name = Security::redact_sensitive_text(account.display_name);
    auto id = account.student_id.empty() ? std::string{} : std::string{"[REDACTED]"};
    if (name.empty() && id.empty()) return "session restored";
    if (name.empty()) return id;
    if (id.empty()) return name;
    return name + " (" + id + ")";
}

std::string lowercase_ascii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string mime_for_path(const std::filesystem::path &path) {
    const auto ext = lowercase_ascii(path.extension().string());
    if (ext == ".txt" || ext == ".log" || ext == ".md") return "text/plain";
    if (ext == ".json") return "application/json";
    if (ext == ".pdf") return "application/pdf";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".zip") return "application/zip";
    return "application/octet-stream";
}

std::string redact_error(const Error &error) {
    return Security::redact_sensitive_text(error.message);
}

std::string cloud_vfs_operation_text(CloudVfs::CloudVfsWriteOperation operation) {
    switch (operation) {
    case CloudVfs::CloudVfsWriteOperation::CreateDirectory: return "mkdir";
    case CloudVfs::CloudVfsWriteOperation::Rename: return "rename";
    case CloudVfs::CloudVfsWriteOperation::Move: return "move";
    case CloudVfs::CloudVfsWriteOperation::Delete: return "delete";
    case CloudVfs::CloudVfsWriteOperation::Upload: return "upload";
    case CloudVfs::CloudVfsWriteOperation::ClearCache: return "clear-cache";
    }
    return "write";
}

std::string cloud_vfs_status_text(CloudVfs::CloudVfsTaskStatus status) {
    switch (status) {
    case CloudVfs::CloudVfsTaskStatus::Pending: return "pending";
    case CloudVfs::CloudVfsTaskStatus::Running: return "running";
    case CloudVfs::CloudVfsTaskStatus::Succeeded: return "succeeded";
    case CloudVfs::CloudVfsTaskStatus::Failed: return "failed";
    case CloudVfs::CloudVfsTaskStatus::Cancelled: return "cancelled";
    }
    return "unknown";
}

int cloud_vfs_progress(CloudVfs::CloudVfsTaskStatus status) {
    switch (status) {
    case CloudVfs::CloudVfsTaskStatus::Pending: return 0;
    case CloudVfs::CloudVfsTaskStatus::Running: return 50;
    case CloudVfs::CloudVfsTaskStatus::Succeeded:
    case CloudVfs::CloudVfsTaskStatus::Failed:
    case CloudVfs::CloudVfsTaskStatus::Cancelled: return 100;
    }
    return 0;
}

CloudBrowserTask cloud_browser_task_from_vfs(const CloudVfs::CloudVfsTask &task) {
    return CloudBrowserTask{task.id,
                            cloud_vfs_operation_text(task.operation),
                            Security::redact_sensitive_text(task.name),
                            task.path,
                            cloud_vfs_progress(task.status),
                            cloud_vfs_status_text(task.status),
                            Security::redact_sensitive_text(task.error_message)};
}

RuntimeFeatureRow runtime_feature_row(const Model::FeatureRecord &record) {
    RuntimeFeatureRow row;
    row.id = Security::redact_sensitive_text(record.id);
    row.title = Security::redact_sensitive_text(record.title);
    row.status = Security::redact_sensitive_text(record.status);
    for (const auto &[key, value] : record.fields) {
        auto redacted_key = Security::redact_sensitive_text(key);
        auto redacted_value = Security::redact_sensitive_text(value);
        if (redacted_key.empty()) continue;
        row.details.push_back(redacted_key + "=" + redacted_value);
    }
    if (!row.details.empty()) row.subtitle = row.details.front();
    return row;
}

RuntimeFeatureState runtime_feature_state(std::string title, std::vector<Model::FeatureRecord> records) {
    RuntimeFeatureState state;
    state.title = Security::redact_sensitive_text(title);
    state.status = records.empty() ? "No records" : "Ready";
    state.rows.reserve(records.size());
    for (const auto &record : records) state.rows.push_back(runtime_feature_row(record));
    return state;
}

RuntimeFeatureState runtime_feature_state(std::string title, const Model::FeatureRecord &record) {
    RuntimeFeatureState state;
    state.title = Security::redact_sensitive_text(title);
    state.status = "Ready";
    state.rows.push_back(runtime_feature_row(record));
    return state;
}

RuntimeFeatureState runtime_feature_state(std::string title, const Model::MutationResult &mutation) {
    RuntimeFeatureState state;
    state.title = Security::redact_sensitive_text(title);
    state.status = mutation.accepted ? "Accepted" : "Rejected";
    state.rows.push_back(runtime_feature_row(mutation.summary));
    if (!mutation.message.empty()) state.rows.back().details.push_back("message=" + Security::redact_sensitive_text(mutation.message));
    return state;
}

std::string exam_status_text(Model::ExamStatus status) {
    switch (status) {
    case Model::ExamStatus::Pending: return "pending";
    case Model::ExamStatus::Arranged: return "arranged";
    case Model::ExamStatus::Finished: return "finished";
    }
    return "unknown";
}

RuntimeFeatureRow course_feature_row(const Model::Course &course) {
    RuntimeFeatureRow row;
    row.id = Security::redact_sensitive_text(course.id.empty() ? course.course_code : course.id);
    row.title = Security::redact_sensitive_text(course.name);
    row.subtitle = Security::redact_sensitive_text(course.teacher + " · " + course.classroom);
    row.status = "week " + std::to_string(course.week_start) + "-" + std::to_string(course.week_end);
    row.details.push_back("day=" + std::to_string(course.day_of_week));
    row.details.push_back("section=" + std::to_string(course.section_start) + "-" + std::to_string(course.section_end));
    if (!course.begin_time.empty() || !course.end_time.empty()) row.details.push_back("time=" + Security::redact_sensitive_text(course.begin_time + "-" + course.end_time));
    if (!course.credit.empty()) row.details.push_back("credit=" + Security::redact_sensitive_text(course.credit));
    return row;
}

RuntimeFeatureRow exam_feature_row(const Model::Exam &exam) {
    RuntimeFeatureRow row;
    row.id = Security::redact_sensitive_text(exam.id.empty() ? exam.course_no : exam.id);
    row.title = Security::redact_sensitive_text(exam.course_name);
    row.subtitle = Security::redact_sensitive_text(exam.time_text.empty() ? exam.exam_date : exam.time_text);
    row.status = exam_status_text(exam.status);
    if (!exam.location.empty()) row.details.push_back("location=" + Security::redact_sensitive_text(exam.location));
    if (!exam.seat_no.empty()) row.details.push_back("seat=" + Security::redact_sensitive_text(exam.seat_no));
    if (!exam.exam_type.empty()) row.details.push_back("type=" + Security::redact_sensitive_text(exam.exam_type));
    if (!exam.start_time.empty() || !exam.end_time.empty()) row.details.push_back("time=" + Security::redact_sensitive_text(exam.start_time + "-" + exam.end_time));
    return row;
}

RuntimeFeatureRow grade_feature_row(const Model::Grade &grade) {
    RuntimeFeatureRow row;
    row.id = Security::redact_sensitive_text(grade.id.empty() ? grade.course_code : grade.id);
    row.title = Security::redact_sensitive_text(grade.course_name);
    row.subtitle = Security::redact_sensitive_text(grade.course_type);
    row.status = Security::redact_sensitive_text(grade.score);
    if (!grade.credit.empty()) row.details.push_back("credit=" + Security::redact_sensitive_text(grade.credit));
    if (!grade.grade_point.empty()) row.details.push_back("gpa=" + Security::redact_sensitive_text(grade.grade_point));
    if (!grade.term_code.empty()) row.details.push_back("term=" + Security::redact_sensitive_text(grade.term_code));
    if (!grade.raw_status.empty()) row.details.push_back("status=" + Security::redact_sensitive_text(grade.raw_status));
    return row;
}

RuntimeFeatureRow account_feature_row(const Model::Account &account) {
    RuntimeFeatureRow row;
    row.id = "account";
    row.title = Security::redact_sensitive_text(account.display_name.empty() ? "Current account" : account.display_name);
    row.subtitle = account.student_id.empty() ? "session restored" : "student=[REDACTED]";
    row.status = account.access_token.empty() ? "session" : "authenticated";
    return row;
}

std::string join_sections(const std::vector<int> &sections) {
    std::ostringstream out;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (i > 0) out << ',';
        out << sections[i];
    }
    return out.str();
}

RuntimeFeatureState classroom_feature_state(const Model::ClassroomQueryResult &result) {
    RuntimeFeatureState state;
    state.title = "Classrooms";
    state.status = result.buildings.empty() ? "No records" : "Ready";
    for (const auto &[building, rooms] : result.buildings) {
        for (const auto &room : rooms) {
            RuntimeFeatureRow row;
            row.id = Security::redact_sensitive_text(room.id);
            row.title = Security::redact_sensitive_text(room.name.empty() ? room.id : room.name);
            row.subtitle = Security::redact_sensitive_text(building);
            row.status = "free";
            if (!room.floor_id.empty()) row.details.push_back("floor=" + Security::redact_sensitive_text(room.floor_id));
            row.details.push_back("sections=" + join_sections(room.free_sections));
            state.rows.push_back(std::move(row));
        }
    }
    return state;
}

template <typename T, typename Mapper>
RuntimeFeatureState typed_feature_state(std::string title, const std::vector<T> &items, Mapper mapper) {
    RuntimeFeatureState state;
    state.title = Security::redact_sensitive_text(title);
    state.status = items.empty() ? "No records" : "Ready";
    state.rows.reserve(items.size());
    for (const auto &item : items) state.rows.push_back(mapper(item));
    return state;
}

bool feature_result_failed(const RuntimeFeatureState &state) {
    return state.rows.size() == 1 && state.rows.front().status == "error";
}

bool contains_filter(const CloudVfs::CloudVfsNode &node, const std::string &filter) {
    if (filter.empty()) return true;
    return lowercase_ascii(node.name).find(lowercase_ascii(filter)) != std::string::npos;
}

std::string mount_dependency_message(CloudMountFrontend frontend, bool available) {
    if (available) return "available";
    return CloudMountManager::frontend_name(frontend) + " dependency is not available in this build/runtime";
}

nlohmann::json mount_dependency_json(CloudMountFrontend frontend) {
    const auto available = CloudMountManager::dependency_available(frontend);
    return {
        {"available", available},
        {"message", mount_dependency_message(frontend, available)},
    };
}

class RuntimeFileUploadSource final : public IUploadSource {
public:
    explicit RuntimeFileUploadSource(std::filesystem::path path)
        : m_path(std::move(path)), m_input(m_path, std::ios::binary) {}

    [[nodiscard]] bool is_open() const { return m_input.is_open(); }
    [[nodiscard]] std::string name() const override { return m_path.filename().string(); }
    [[nodiscard]] std::string content_type() const override { return mime_for_path(m_path); }

    [[nodiscard]] Result<std::uint64_t> size() override {
        std::error_code ec;
        const auto length = std::filesystem::file_size(m_path, ec);
        if (ec) return make_error(ErrorCode::InvalidArgument, "无法读取上传文件大小");
        return static_cast<std::uint64_t>(length);
    }

    [[nodiscard]] Result<void> rewind() override {
        if (!m_input.is_open()) return make_error(ErrorCode::InvalidArgument, "无法读取上传文件");
        m_input.clear();
        m_input.seekg(0, std::ios::beg);
        if (!m_input) return make_error(ErrorCode::InvalidArgument, "无法回退上传文件流");
        return {};
    }

    [[nodiscard]] Result<std::size_t> read(unsigned char *buffer, std::size_t max_bytes) override {
        if (max_bytes == 0) return static_cast<std::size_t>(0);
        if (!m_input.is_open()) return make_error(ErrorCode::InvalidArgument, "无法读取上传文件");
        m_input.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(max_bytes));
        const auto count = static_cast<std::size_t>(m_input.gcount());
        if (m_input.bad()) return make_error(ErrorCode::InvalidArgument, "读取上传文件失败");
        return count;
    }

private:
    std::filesystem::path m_path;
    std::ifstream m_input;
};

} // namespace

std::filesystem::path default_app_data_dir() {
    auto override_dir = env_path("UBAANEXT_APP_DATA_DIR");
    if (!override_dir.empty()) return override_dir;

#if defined(_WIN32)
    auto local_app_data = env_path("LOCALAPPDATA");
    if (!local_app_data.empty()) return local_app_data / "UBAANext";
    auto user_profile = env_path("USERPROFILE");
    if (!user_profile.empty()) return user_profile / ".ubaanext";
#else
    auto xdg_data_home = env_path("XDG_DATA_HOME");
    if (!xdg_data_home.empty()) return xdg_data_home / "ubaanext";
    auto home = env_path("HOME");
    if (!home.empty()) return home / ".ubaanext";
#endif
    return ".ubaanext";
}

std::filesystem::path default_cache_dir(const std::filesystem::path &app_data_dir) {
    auto override_dir = env_path("UBAANEXT_CACHE_DIR");
    if (!override_dir.empty()) return override_dir;
    return app_data_dir / "cache";
}

std::filesystem::path default_log_dir(const std::filesystem::path &app_data_dir) {
    auto override_dir = env_path("UBAANEXT_LOG_DIR");
    if (!override_dir.empty()) return override_dir;
    return app_data_dir / "logs";
}

std::filesystem::path session_file_path(const std::filesystem::path &app_data_dir) {
    return app_data_dir / "session.dat";
}

std::filesystem::path cookie_file_path(const std::filesystem::path &app_data_dir) {
    return app_data_dir / "cookies.dat";
}

std::filesystem::path config_file_path(const std::filesystem::path &app_data_dir) {
    return app_data_dir / "config.json";
}

RuntimeContext::RuntimeContext(AppContext context, std::filesystem::path app_data_dir, std::filesystem::path cache_dir)
    : m_context(std::move(context)),
      m_app_data_dir(std::move(app_data_dir)),
      m_cache_dir(std::move(cache_dir)),
      m_vfs_content_cache(std::make_unique<CloudVfs::MemoryCloudVfsContentCache>()),
      m_runtime_vfs_gate(std::make_unique<CloudVfs::AllowAllCloudVfsWriteGate>()),
      m_mount_manager(std::make_unique<CloudMountManager>()) {
    rebuild_cloud_vfs();
    m_mount_manager->register_adapter(create_winfsp_cloud_mount_adapter());
    m_mount_manager->register_adapter(create_cloud_files_mount_adapter());
    m_mount_manager->register_adapter(create_fuse_cloud_mount_adapter());
}

void RuntimeContext::rebuild_cloud_vfs() {
    if (m_mount_manager) m_mount_manager->stop_all();
    if (m_vfs_content_cache) m_vfs_content_cache->clear();
    auto factory = ServiceFactory(m_context);
    m_cloud_service = std::make_unique<CloudService>(factory.create_cloud_service());
    CloudVfs::CloudVfsConfig vfs_config;
    vfs_config.read_only = false;
    m_cloud_vfs = std::make_unique<CloudVfs::CloudVfs>(*m_cloud_service, *m_vfs_content_cache, vfs_config);
    m_cloud_vfs->set_write_gate(m_runtime_vfs_gate.get());
    if (m_mount_manager) m_mount_manager->set_vfs(*m_cloud_vfs);
    m_cloud_current_path = "/";
    m_cloud_current_items.clear();
}

Result<RuntimeContext> RuntimeContext::create(const RuntimeOptions &options) {
    auto app_dir = options.app_data_dir.empty() ? default_app_data_dir() : options.app_data_dir;
    auto config_path = config_file_path(app_dir);
    auto config = options.config_provided ? options.config : CliConfig::load(config_path.string());
    auto mode = options.mode.empty() ? config.mode : options.mode;

    UBAANextCli::PlatformContextOptions platform_options;
    platform_options.mock = options.mock;
    platform_options.mode = mode;
    platform_options.config = config;
    platform_options.session_file_path = session_file_path(app_dir);
    platform_options.cookie_file_path = cookie_file_path(app_dir);

    auto context = UBAANextCli::create_current_platform_context(platform_options);
    return RuntimeContext(std::move(context), app_dir, default_cache_dir(app_dir));
}

AppContext &RuntimeContext::context() {
    return m_context;
}

const AppContext &RuntimeContext::context() const {
    return m_context;
}

ServiceFactory RuntimeContext::services() {
    return ServiceFactory(m_context);
}

CloudVfs::CloudVfs &RuntimeContext::cloud_vfs() {
    return *m_cloud_vfs;
}

CloudMountManager &RuntimeContext::mounts() {
    return *m_mount_manager;
}

void RuntimeContext::configure_cloud_write_gate(bool confirmed, const std::string &operation) {
    m_cloud_service->set_write_operation_gate(confirmed_write_operation(m_context.capabilities, operation, confirmed));
}

CloudBrowserState RuntimeContext::cloud_state(const std::string &filter) const {
    CloudBrowserState state;
    state.current_path = m_cloud_current_path;
    state.breadcrumb = m_cloud_current_path;
    if (auto current = m_cloud_vfs->lookup(m_cloud_current_path)) state.current_docid = current->docid;
    for (const auto &node : m_cloud_current_items) {
        if (!contains_filter(node, filter)) continue;
        CloudBrowserItem item;
        item.docid = node.docid;
        item.name = Security::redact_sensitive_text(node.name);
        item.path = node.path.value;
        item.is_dir = node.is_dir;
        item.size = node.size;
        item.updated_at = node.mtime;
        state.items.push_back(std::move(item));
    }
    for (const auto &task : m_cloud_vfs->tasks()) state.tasks.push_back(cloud_browser_task_from_vfs(task));
    state.tasks.insert(state.tasks.end(), m_cloud_tasks.begin(), m_cloud_tasks.end());
    state.status = state.items.empty() ? "Cloud directory is empty" : "Cloud ready";
    return state;
}

Result<CloudBrowserState> RuntimeContext::cloud_open_root() {
    auto root = m_cloud_vfs->load_user_root();
    if (!root) return make_error(root.error().code, redact_error(root.error()));
    m_cloud_current_path = "/";
    auto children = m_cloud_vfs->list(m_cloud_current_path, true);
    if (!children) return make_error(children.error().code, redact_error(children.error()));
    m_cloud_current_items = *children;
    save_cookies();
    return cloud_state();
}

Result<CloudBrowserState> RuntimeContext::cloud_open_path(const std::string &path, const std::string &filter) {
    auto normalized = normalize_cloud_browser_path(path);
    if (!normalized) return make_error(normalized.error().code, redact_error(normalized.error()));
    if (*normalized == "/") {
        auto root = cloud_open_root();
        if (!root) return make_error(root.error().code, root.error().message);
        return cloud_state(filter);
    }

    if (!m_cloud_vfs->root()) {
        auto root = m_cloud_vfs->load_user_root();
        if (!root) return make_error(root.error().code, redact_error(root.error()));
    }

    std::string current_path = "/";
    auto parts = cloud_browser_path_parts(*normalized);
    for (const auto &part : parts) {
        auto child = find_child_dir(*m_cloud_vfs, current_path, part);
        if (!child) return make_error(child.error().code, redact_error(child.error()));
        current_path = child->path.value;
    }

    auto children = m_cloud_vfs->list(current_path);
    if (!children) return make_error(children.error().code, redact_error(children.error()));
    m_cloud_current_path = current_path;
    m_cloud_current_items = *children;
    save_cookies();
    return cloud_state(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_refresh(const std::string &filter) {
    auto current = m_cloud_vfs->lookup(m_cloud_current_path);
    if (!current) {
        auto root = cloud_open_root();
        if (!root) return make_error(root.error().code, root.error().message);
        return cloud_state(filter);
    }
    auto children = m_cloud_vfs->refresh(m_cloud_current_path);
    if (!children) return make_error(children.error().code, redact_error(children.error()));
    m_cloud_current_items = *children;
    save_cookies();
    return cloud_state(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_create_dir(const std::string &name, bool confirmed, const std::string &filter) {
    configure_cloud_write_gate(confirmed, "file mkdir");
    auto result = m_cloud_vfs->create_directory(m_cloud_current_path, name);
    if (!result) return make_error(result.error().code, redact_error(result.error()));
    return cloud_refresh(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_rename(const std::string &path, const std::string &new_name, bool confirmed, const std::string &filter) {
    configure_cloud_write_gate(confirmed, "file rename");
    auto result = m_cloud_vfs->rename(path, new_name);
    if (!result) return make_error(result.error().code, redact_error(result.error()));
    return cloud_refresh(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_delete(const std::string &path, bool confirmed, const std::string &filter) {
    configure_cloud_write_gate(confirmed, "file delete");
    auto result = m_cloud_vfs->remove(path);
    if (!result) return make_error(result.error().code, redact_error(result.error()));
    return cloud_refresh(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_upload_file(const std::filesystem::path &local_path, bool overwrite, bool confirmed, const std::string &filter) {
    auto source = std::make_shared<RuntimeFileUploadSource>(local_path);
    if (!source->is_open()) return make_error(ErrorCode::InvalidArgument, "无法读取上传文件");
    configure_cloud_write_gate(confirmed, "file upload");
    auto policy = overwrite ? CloudVfs::CloudVfsConflictPolicy::Overwrite : CloudVfs::CloudVfsConflictPolicy::UseSuggestedName;
    auto queued = m_cloud_vfs->enqueue_upload(m_cloud_current_path, source->name(), source, policy);
    if (!queued) return make_error(queued.error().code, redact_error(queued.error()));
    auto processed = m_cloud_vfs->process_next_upload();
    if (!processed) return make_error(processed.error().code, redact_error(processed.error()));
    if (processed->status == CloudVfs::CloudVfsTaskStatus::Failed) return make_error(ErrorCode::NetworkError, Security::redact_sensitive_text(processed->error_message));
    return cloud_refresh(filter);
}

Result<CloudBrowserState> RuntimeContext::cloud_download_file(const std::string &path, const std::filesystem::path &local_path, bool overwrite, const std::string &filter) {
    auto node = m_cloud_vfs->lookup(path);
    if (!node) return make_error(node.error().code, redact_error(node.error()));
    if (node->is_dir) return make_error(ErrorCode::InvalidArgument, "桌面端暂不支持下载目录");
    std::error_code ec;
    if (std::filesystem::exists(local_path, ec) && !overwrite) return make_error(ErrorCode::InvalidArgument, "目标文件已存在，需要确认覆盖");
    const auto parent_path = local_path.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path, ec);
        if (ec) return make_error(ErrorCode::StorageError, Security::redact_sensitive_text(ec.message()));
    }
    auto task_id = m_next_cloud_task_id++;
    m_cloud_tasks.push_back({task_id, "download", Security::redact_sensitive_text(node->name), node->path.value, 0, "running", {}});
    std::ofstream output(local_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) return make_error(ErrorCode::StorageError, "无法写入下载文件");
    constexpr std::uint64_t chunk_size = 1024ULL * 1024ULL;
    std::uint64_t offset = 0;
    while (offset < node->size) {
        const auto take = std::min(chunk_size, node->size - offset);
        auto chunk = m_cloud_vfs->read(node->path.value, offset, take);
        if (!chunk) {
            auto &task = m_cloud_tasks.back();
            task.status = "failed";
            task.message = redact_error(chunk.error());
            return make_error(chunk.error().code, task.message);
        }
        output.write(reinterpret_cast<const char *>(chunk->data()), static_cast<std::streamsize>(chunk->size()));
        if (!output) return make_error(ErrorCode::StorageError, "写入下载文件失败");
        offset += static_cast<std::uint64_t>(chunk->size());
        m_cloud_tasks.back().progress = node->size == 0 ? 100 : static_cast<int>((offset * 100ULL) / node->size);
        if (chunk->empty()) break;
    }
    auto &task = m_cloud_tasks.back();
    task.progress = 100;
    task.status = "succeeded";
    task.message = "download completed";
    save_cookies();
    return cloud_state(filter);
}

RuntimeDiagnostics RuntimeContext::diagnostics() const {
    RuntimeDiagnostics diagnostics;
    diagnostics.version = UBAANEXT_VERSION_STRING;
    diagnostics.mode = mode_text(m_context.conn_mode);
    diagnostics.mock = m_context.mock_mode;
    diagnostics.secure_store = m_context.capabilities.secure_store;
    diagnostics.cookie_persistence = m_context.capabilities.cookie_persistence;
    diagnostics.live_login = m_context.capabilities.live_login;
    diagnostics.write_operations = m_context.capabilities.write_operations;
    diagnostics.winfsp_available = CloudMountManager::dependency_available(CloudMountFrontend::WinFsp);
    diagnostics.cloud_files_available = CloudMountManager::dependency_available(CloudMountFrontend::CloudFiles);
    diagnostics.fuse_available = CloudMountManager::dependency_available(CloudMountFrontend::Fuse);
    diagnostics.credential_persistence_available = m_context.credential_persistence_available;
    diagnostics.credential_persistence_secure = m_context.credential_persistence_secure;
    diagnostics.credential_persistence_plaintext_fallback = m_context.credential_persistence_plaintext_fallback;
    diagnostics.app_data_dir = m_app_data_dir;
    diagnostics.config_file = config_file_path(m_app_data_dir);
    diagnostics.session_file = session_file_path(m_app_data_dir);
    diagnostics.cookie_file = cookie_file_path(m_app_data_dir);
    diagnostics.cache_dir = m_cache_dir;
    diagnostics.log_dir = default_log_dir(m_app_data_dir);
    diagnostics.cache_enabled = m_context.config.cache_enabled;
    diagnostics.cache_size_bytes = cache_size_bytes();
    if (m_context.store) {
        auto factory = ServiceFactory(const_cast<AppContext &>(m_context));
        auto auth = factory.create_auth_service();
        if (auto account = auth.restore_session()) {
            diagnostics.session_present = true;
            diagnostics.account_summary = account_summary(*account);
        }
    }
    return diagnostics;
}

Result<std::string> RuntimeContext::diagnostics_json() const {
    const auto diag = diagnostics();
    nlohmann::json mounts = nlohmann::json::array();
    for (const auto &status : m_mount_manager->statuses()) {
        mounts.push_back({
            {"frontend", CloudMountManager::frontend_name(status.frontend)},
            {"running", status.running},
            {"writable", status.writable},
            {"dependencyAvailable", status.dependency_available},
            {"mountPoint", redact_path(status.mount_point)},
            {"message", Security::redact_sensitive_text(status.message)},
        });
    }

    nlohmann::json json = {
        {"version", diag.version},
        {"mode", diag.mode},
        {"mock", diag.mock},
        {"account", {
            {"sessionPresent", diag.session_present},
            {"summary", Security::redact_sensitive_text(diag.account_summary)},
        }},
        {"capabilities", {
            {"secureStore", diag.secure_store},
            {"cookiePersistence", diag.cookie_persistence},
            {"liveLogin", diag.live_login},
            {"writeOperations", diag.write_operations},
            {"credentialPersistence", diag.credential_persistence_available},
            {"credentialPersistenceSecure", diag.credential_persistence_secure},
            {"credentialPlaintextFallback", diag.credential_persistence_plaintext_fallback},
            {"winfsp", diag.winfsp_available},
            {"cloudFiles", diag.cloud_files_available},
            {"fuse", diag.fuse_available},
        }},
        {"paths", {
            {"appDataDir", redact_path(diag.app_data_dir)},
            {"configFile", redact_path(diag.config_file)},
            {"sessionFile", redact_path(diag.session_file)},
            {"cookieFile", redact_path(diag.cookie_file)},
            {"cacheDir", redact_path(diag.cache_dir)},
            {"logDir", redact_path(diag.log_dir)},
        }},
        {"session", {
            {"present", diag.session_present},
            {"storageAvailable", diag.credential_persistence_available},
            {"storageSecure", diag.credential_persistence_secure},
            {"plaintextFallback", diag.credential_persistence_plaintext_fallback},
        }},
        {"cache", {
            {"enabled", diag.cache_enabled},
            {"dir", redact_path(diag.cache_dir)},
            {"sizeBytes", diag.cache_size_bytes},
        }},
        {"dependencies", {
            {"mounts", {
                {"winfsp", mount_dependency_json(CloudMountFrontend::WinFsp)},
                {"cloudFiles", mount_dependency_json(CloudMountFrontend::CloudFiles)},
                {"fuse", mount_dependency_json(CloudMountFrontend::Fuse)},
            }},
        }},
        {"mounts", mounts},
    };
    return json.dump(2);
}

Result<Model::Account> RuntimeContext::login(const LoginRequest &request) {
    if (request.username.empty()) return make_error(ErrorCode::InvalidArgument, "login requires account");
    if (request.password.empty()) return make_error(ErrorCode::InvalidArgument, "login requires password");
    auto auth = services().create_auth_service();
#if UBAANEXT_ENABLE_MOCKS
    if (m_context.mock_mode) {
        auto result = auth.login_mock(request.username, request.password);
        if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
        if (m_context.store) {
            m_context.store->set_string("login.username", request.username);
            m_context.store->set_string("login.password", request.password);
            m_context.store->set_string("login.connection_mode", "mock");
            if (auto flushed = m_context.store->flush(); !flushed) return make_error(flushed.error().code, flushed.error().message);
        }
        rebuild_cloud_vfs();
        return *result;
    }
#endif
    if (!m_context.credential_persistence_available || !m_context.store) {
        return make_error(ErrorCode::UnsupportedSecureStore, "session persistence is not available");
    }
    auto result = auth.login_real(request.username, request.password, m_context.conn_mode);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    m_context.store->set_string("login.username", request.username);
    m_context.store->set_string("login.password", request.password);
    m_context.store->set_string("login.connection_mode", mode_text(m_context.conn_mode));
    if (auto flushed = m_context.store->flush(); !flushed) return make_error(flushed.error().code, flushed.error().message);
    rebuild_cloud_vfs();
    return *result;
}

Result<Model::Account> RuntimeContext::whoami() {
    auto auth = services().create_auth_service();
    auto result = auth.restore_session();
    if (!result) return make_error(ErrorCode::SessionExpired, "not logged in");
    return *result;
}

Result<RuntimeFeatureState> RuntimeContext::today_courses() {
    auto service = services().create_course_service();
    auto result = service.get_today_courses();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return typed_feature_state("Today courses", *result, course_feature_row);
}

Result<RuntimeFeatureState> RuntimeContext::date_courses(const std::string &date) {
    if (date.empty()) return make_error(ErrorCode::InvalidArgument, "date is required");
    auto service = services().create_course_service();
    auto result = service.get_date_courses(date);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return typed_feature_state("Courses " + date, *result, course_feature_row);
}

Result<RuntimeFeatureState> RuntimeContext::week_courses(int week, const std::string &term_code) {
    if (week <= 0) return make_error(ErrorCode::InvalidArgument, "week must be positive");
    auto service = services().create_course_service();
    auto result = term_code.empty() ? service.get_week_courses(week) : service.get_week_courses(week, term_code);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return typed_feature_state("Week " + std::to_string(week) + " courses", *result, course_feature_row);
}

Result<RuntimeFeatureState> RuntimeContext::exams(const std::string &term_code) {
    auto service = services().create_exam_service();
    auto result = service.get_exams(term_code);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return typed_feature_state(term_code.empty() ? "Exams" : "Exams " + term_code, *result, exam_feature_row);
}

Result<RuntimeFeatureState> RuntimeContext::grades(const std::string &term_code, bool all_terms) {
    auto service = services().create_grade_service();
    auto result = all_terms ? service.list_all_grades() : service.list_grades(term_code);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return typed_feature_state(all_terms ? "All grades" : "Grades " + term_code, *result, grade_feature_row);
}

Result<RuntimeFeatureState> RuntimeContext::user_info_state() {
    auto account = whoami();
    if (account) {
        RuntimeFeatureState state;
        state.title = "User info";
        state.status = "Ready";
        state.rows.push_back(account_feature_row(*account));
        return state;
    }

    auto service = services().create_feature_service();
    auto result = service.user_info();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("User info", *result);
}

Result<RuntimeFeatureState> RuntimeContext::todos(bool pending_only) {
    auto service = services().create_todo_service();
    TodoQuery query;
    query.pending_only = pending_only;
    auto result = service.list_todos(query);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state(pending_only ? "Pending todos" : "Todos", *result);
}

Result<RuntimeFeatureState> RuntimeContext::classrooms(int campus_id, const std::string &date, const std::vector<int> &sections) {
    if (campus_id <= 0) return make_error(ErrorCode::InvalidArgument, "campus id must be positive");
    if (date.empty()) return make_error(ErrorCode::InvalidArgument, "date is required");
    auto service = services().create_classroom_service();
    auto result = sections.empty() ? service.query_classrooms(campus_id, date)
                                   : service.query_classrooms(campus_id, date, sections);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return classroom_feature_state(*result);
}

Result<RuntimeFeatureState> RuntimeContext::spoc_assignments(bool pending_only) {
    auto service = services().create_spoc_service();
    SpocAssignmentQuery query;
    query.pending_only = pending_only;
    auto result = service.list_assignments(query);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state(pending_only ? "Pending SPOC assignments" : "SPOC assignments", *result);
}

Result<RuntimeFeatureState> RuntimeContext::judge_assignments(const std::string &course_id) {
    auto service = services().create_judge_service();
    auto result = course_id.empty() ? service.list_assignments(JudgeAssignmentQuery{}) : service.list_assignments(course_id);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("XiJi assignments", *result);
}

Result<RuntimeFeatureState> RuntimeContext::bykc_courses() {
    auto service = services().create_bykc_service();
    auto result = service.courses();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("Boya courses", *result);
}

Result<RuntimeFeatureState> RuntimeContext::venue_sites() {
    auto service = services().create_venue_reservation_service();
    auto result = service.list_sites();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("Seminar sites", *result);
}

Result<RuntimeFeatureState> RuntimeContext::library_libraries(const std::string &day) {
    if (day.empty()) return make_error(ErrorCode::InvalidArgument, "day is required");
    auto service = services().create_library_seat_service();
    auto result = service.list_libraries(day);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("Library", *result);
}

Result<RuntimeFeatureState> RuntimeContext::ygdk_overview() {
    auto service = services().create_ygdk_service();
    auto result = service.overview();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("YGDK", *result);
}

Result<RuntimeFeatureState> RuntimeContext::evaluations() {
    auto service = services().create_evaluation_service();
    auto result = service.list_evaluations();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state("Evaluations", *result);
}

Result<RuntimeFeatureState> RuntimeContext::feature_list(const RuntimeFeatureQuery &query) {
    if (query.domain.empty()) return make_error(ErrorCode::InvalidArgument, "feature domain is required");
    if (query.operation.empty()) return make_error(ErrorCode::InvalidArgument, "feature operation is required");
    auto service = services().create_feature_service();
    auto result = service.list(query.domain, query.operation);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state(query.domain + " " + query.operation, *result);
}

Result<RuntimeFeatureState> RuntimeContext::feature_show(const RuntimeFeatureQuery &query) {
    if (query.domain.empty()) return make_error(ErrorCode::InvalidArgument, "feature domain is required");
    if (query.operation.empty()) return make_error(ErrorCode::InvalidArgument, "feature operation is required");
    auto service = services().create_feature_service();
    auto result = service.show(query.domain, query.operation, query.id);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state(query.domain + " " + query.operation, *result);
}

Result<RuntimeFeatureState> RuntimeContext::feature_mutate(const RuntimeFeatureQuery &query) {
    if (query.domain.empty()) return make_error(ErrorCode::InvalidArgument, "feature domain is required");
    if (query.operation.empty()) return make_error(ErrorCode::InvalidArgument, "feature operation is required");
    auto allowed = require_write_operation(confirmed_write_operation(m_context.capabilities, query.domain + " " + query.operation, query.confirmed));
    if (!allowed) return make_error(allowed.error().code, Security::redact_sensitive_text(allowed.error().message));
    auto service = services().create_feature_service();
    auto result = service.mutate(query.domain, query.operation, query.id, query.confirmed);
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    save_cookies();
    return runtime_feature_state(query.domain + " " + query.operation, *result);
}

Result<void> RuntimeContext::logout() {
    auto auth = services().create_auth_service();
    auto result = auth.logout();
    if (!result) return make_error(result.error().code, Security::redact_sensitive_text(result.error().message));
    clear_cookies();
    if (m_context.cache) m_context.cache->clear();
    return {};
}

Result<void> RuntimeContext::set_connection_mode(std::string mode) {
    if (mode != "direct" && mode != "vpn" && mode != "mock") {
        return make_error(ErrorCode::InvalidArgument, "connection mode must be direct, vpn, or mock");
    }
#if UBAANEXT_ENABLE_MOCKS
    if (mode == "mock") {
        m_context.mock_mode = true;
        m_context.conn_mode = ConnectionMode::Mock;
    } else
#endif
    {
        m_context.mock_mode = false;
        m_context.conn_mode = mode == "direct" ? ConnectionMode::Direct : ConnectionMode::WebVPN;
    }
    m_context.config.mode = std::move(mode);
    m_context.config.save(config_file_path(m_app_data_dir).string());
    return {};
}

Result<void> RuntimeContext::clear_cache(bool confirmed) {
    WriteOperationGate gate;
    gate.confirmed = confirmed;
    gate.allow_write_operations = true;
    gate.operation = "clear cache";
    auto allowed = require_write_operation(gate);
    if (!allowed) return make_error(allowed.error().code, Security::redact_sensitive_text(allowed.error().message));
    auto cleared = m_cloud_vfs->clear_content_cache();
    if (!cleared) return make_error(cleared.error().code, cleared.error().message);
    if (m_context.cache) m_context.cache->clear();
    std::error_code ec;
    if (std::filesystem::exists(m_cache_dir, ec)) {
        std::filesystem::remove_all(m_cache_dir, ec);
        if (ec) return make_error(ErrorCode::StorageError, Security::redact_sensitive_text(ec.message()));
    }
    return {};
}

std::uintmax_t RuntimeContext::cache_size_bytes() const {
    return directory_size(m_cache_dir);
}

void RuntimeContext::save_cookies() {
    UBAANextCli::save_platform_cookies(m_context);
}

void RuntimeContext::clear_cookies() {
    UBAANextCli::clear_platform_cookies(m_context);
}

const std::filesystem::path &RuntimeContext::app_data_dir() const {
    return m_app_data_dir;
}

const std::filesystem::path &RuntimeContext::cache_dir() const {
    return m_cache_dir;
}

} // namespace UBAANext::Runtime
