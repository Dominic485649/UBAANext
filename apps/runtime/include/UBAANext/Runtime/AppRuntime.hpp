#pragma once

#include <UBAANext/CloudVfs/CloudVfs.hpp>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Runtime/CloudMountManager.hpp>

#include "AppContext.hpp"
#include "CliConfig.hpp"
#include "PlatformContextFactory.hpp"
#include "ServiceFactory.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace UBAANext::Runtime {

using AppContext = UBAANextCli::AppContext;
using CliConfig = UBAANextCli::CliConfig;
using ServiceFactory = UBAANextCli::ServiceFactory;

struct RuntimeOptions {
    bool mock = false;
    std::string mode;
    bool config_provided = false;
    CliConfig config;
    std::filesystem::path app_data_dir;
};

struct RuntimeDiagnostics {
    std::string version;
    std::string mode;
    bool mock = false;
    bool secure_store = false;
    bool cookie_persistence = false;
    bool live_login = false;
    bool write_operations = false;
    bool winfsp_available = false;
    bool cloud_files_available = false;
    bool fuse_available = false;
    bool credential_persistence_available = false;
    bool credential_persistence_secure = false;
    bool credential_persistence_plaintext_fallback = false;
    bool session_present = false;
    std::string account_summary;
    std::filesystem::path app_data_dir;
    std::filesystem::path config_file;
    std::filesystem::path session_file;
    std::filesystem::path cookie_file;
    std::filesystem::path cache_dir;
    std::filesystem::path log_dir;
    bool cache_enabled = true;
    std::uintmax_t cache_size_bytes = 0;
};

struct LoginRequest {
    std::string username;
    std::string password;
};

struct CloudBrowserItem {
    std::string docid;
    std::string name;
    std::string path;
    bool is_dir = false;
    std::uint64_t size = 0;
    std::string updated_at;
};

struct CloudBrowserTask {
    std::uint64_t id = 0;
    std::string operation;
    std::string name;
    std::string path;
    int progress = 0;
    std::string status;
    std::string message;
};

struct CloudBrowserState {
    std::string current_path = "/";
    std::string current_docid;
    std::string breadcrumb = "/";
    std::string status;
    std::vector<CloudBrowserItem> items;
    std::vector<CloudBrowserTask> tasks;
};

struct RuntimeFeatureRow {
    std::string id;
    std::string title;
    std::string subtitle;
    std::string status;
    std::vector<std::string> details;
};

struct RuntimeFeatureState {
    std::string title;
    std::string status;
    std::vector<RuntimeFeatureRow> rows;
};

struct RuntimeFeatureQuery {
    std::string domain;
    std::string operation;
    std::string id;
    bool confirmed = false;
};

class RuntimeContext {
public:
    RuntimeContext(RuntimeContext &&) noexcept = default;
    RuntimeContext &operator=(RuntimeContext &&) noexcept = default;
    RuntimeContext(const RuntimeContext &) = delete;
    RuntimeContext &operator=(const RuntimeContext &) = delete;

    [[nodiscard]] static Result<RuntimeContext> create(const RuntimeOptions &options = {});

    [[nodiscard]] AppContext &context();
    [[nodiscard]] const AppContext &context() const;
    [[nodiscard]] ServiceFactory services();
    [[nodiscard]] CloudVfs::CloudVfs &cloud_vfs();
    [[nodiscard]] CloudMountManager &mounts();
    [[nodiscard]] CloudBrowserState cloud_state(const std::string &filter = {}) const;
    [[nodiscard]] Result<CloudBrowserState> cloud_open_root();
    [[nodiscard]] Result<CloudBrowserState> cloud_open_path(const std::string &path, const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_refresh(const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_create_dir(const std::string &name, bool confirmed, const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_rename(const std::string &path, const std::string &new_name, bool confirmed, const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_delete(const std::string &path, bool confirmed, const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_upload_file(const std::filesystem::path &local_path, bool overwrite, bool confirmed, const std::string &filter = {});
    [[nodiscard]] Result<CloudBrowserState> cloud_download_file(const std::string &path, const std::filesystem::path &local_path, bool overwrite, const std::string &filter = {});
    [[nodiscard]] RuntimeDiagnostics diagnostics() const;
    [[nodiscard]] Result<std::string> diagnostics_json() const;
    [[nodiscard]] Result<Model::Account> login(const LoginRequest &request);
    [[nodiscard]] Result<Model::Account> whoami();
    [[nodiscard]] Result<RuntimeFeatureState> today_courses();
    [[nodiscard]] Result<RuntimeFeatureState> date_courses(const std::string &date);
    [[nodiscard]] Result<RuntimeFeatureState> week_courses(int week, const std::string &term_code = {});
    [[nodiscard]] Result<RuntimeFeatureState> exams(const std::string &term_code = {});
    [[nodiscard]] Result<RuntimeFeatureState> grades(const std::string &term_code = {}, bool all_terms = false);
    [[nodiscard]] Result<RuntimeFeatureState> user_info_state();
    [[nodiscard]] Result<RuntimeFeatureState> todos(bool pending_only = true);
    [[nodiscard]] Result<RuntimeFeatureState> classrooms(int campus_id, const std::string &date, const std::vector<int> &sections = {});
    [[nodiscard]] Result<RuntimeFeatureState> spoc_assignments(bool pending_only = false);
    [[nodiscard]] Result<RuntimeFeatureState> judge_assignments(const std::string &course_id = {});
    [[nodiscard]] Result<RuntimeFeatureState> bykc_courses();
    [[nodiscard]] Result<RuntimeFeatureState> venue_sites();
    [[nodiscard]] Result<RuntimeFeatureState> library_libraries(const std::string &day);
    [[nodiscard]] Result<RuntimeFeatureState> ygdk_overview();
    [[nodiscard]] Result<RuntimeFeatureState> evaluations();
    [[nodiscard]] Result<RuntimeFeatureState> feature_list(const RuntimeFeatureQuery &query);
    [[nodiscard]] Result<RuntimeFeatureState> feature_show(const RuntimeFeatureQuery &query);
    [[nodiscard]] Result<RuntimeFeatureState> feature_mutate(const RuntimeFeatureQuery &query);
    [[nodiscard]] Result<void> logout();
    [[nodiscard]] Result<void> set_connection_mode(std::string mode);
    [[nodiscard]] Result<void> clear_cache(bool confirmed);
    [[nodiscard]] std::uintmax_t cache_size_bytes() const;
    void save_cookies();
    void clear_cookies();
    void configure_cloud_write_gate(bool confirmed, const std::string &operation);
    void rebuild_cloud_vfs();

    [[nodiscard]] const std::filesystem::path &app_data_dir() const;
    [[nodiscard]] const std::filesystem::path &cache_dir() const;

private:
    RuntimeContext(AppContext context, std::filesystem::path app_data_dir, std::filesystem::path cache_dir);

    AppContext m_context;
    std::filesystem::path m_app_data_dir;
    std::filesystem::path m_cache_dir;
    std::unique_ptr<CloudVfs::MemoryCloudVfsContentCache> m_vfs_content_cache;
    std::unique_ptr<CloudVfs::AllowAllCloudVfsWriteGate> m_runtime_vfs_gate;
    std::unique_ptr<CloudService> m_cloud_service;
    std::unique_ptr<CloudVfs::CloudVfs> m_cloud_vfs;
    std::unique_ptr<CloudMountManager> m_mount_manager;
    std::string m_cloud_current_path = "/";
    std::vector<CloudVfs::CloudVfsNode> m_cloud_current_items;
    std::vector<CloudBrowserTask> m_cloud_tasks;
    std::uint64_t m_next_cloud_task_id = 1;
};

[[nodiscard]] std::filesystem::path default_app_data_dir();
[[nodiscard]] std::filesystem::path default_cache_dir(const std::filesystem::path &app_data_dir);
[[nodiscard]] std::filesystem::path default_log_dir(const std::filesystem::path &app_data_dir);
[[nodiscard]] std::filesystem::path session_file_path(const std::filesystem::path &app_data_dir);
[[nodiscard]] std::filesystem::path cookie_file_path(const std::filesystem::path &app_data_dir);
[[nodiscard]] std::filesystem::path config_file_path(const std::filesystem::path &app_data_dir);

} // namespace UBAANext::Runtime
