#pragma once

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Model/Cloud.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Net/CookieStore.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Storage/CacheStore.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Upload/UploadSource.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace UBAANext {

enum class CloudRootKind {
    All,
    User,
    Shared,
    Department,
    Group,
};

struct CloudListQuery {
    std::string doc_id;
    std::string token;
};

struct CloudLoginCredentials {
    std::string username;
    std::string password;
};

struct CloudItemRef {
    std::string doc_id;
    std::string name;
    bool is_dir = false;
    std::string token;
};

struct CloudShareRequest {
    std::string item_id;
    std::string title;
    bool is_dir = false;
    std::string expires_at = "1970-01-01T08:00:00+08:00";
    std::string password;
    std::int64_t limit = -1;
    Model::CloudSharePermission permission;
};

struct CloudUploadRequest {
    std::string parent_id;
    std::string name;
    std::string token;
};

class CloudService {
public:
    CloudService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode,
                 std::optional<CloudLoginCredentials> login_credentials = std::nullopt);
    CloudService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode,
                 ICryptoProvider &crypto, std::optional<CloudLoginCredentials> login_credentials = std::nullopt);

    /** ReadOnlyCandidate: lists Anyshare root document libraries. */
    Result<std::vector<Model::CloudItem>> roots(CloudRootKind kind);
    /** ReadOnlyCandidate: returns user's personal document library root as an item. */
    Result<Model::CloudItem> user_root();
    /** ReadOnlyCandidate: lists contents of a cloud directory. */
    Result<Model::CloudDir> list_dir(const CloudListQuery &query);
    /** ReadOnlyCandidate: returns cloud item recursive size metadata. */
    Result<Model::CloudSize> item_size(const CloudListQuery &query);
    /** ReadOnlyCandidate: lists recycle bin contents of user's personal document library. */
    Result<Model::CloudDir> recycle_bin();
    /** ReadOnlyCandidate: lists user's public share records. */
    Result<std::vector<Model::CloudShare>> share_history();
    /** ReadOnlyCandidate: returns a conflict-free name suggested by AnyShare. */
    Result<std::string> suggest_name(const std::string &parent_id, const std::string &name);
    /** ReadOnlyCandidate: lists share records for a single cloud item. */
    Result<std::vector<Model::CloudShare>> share_record(const std::string &item_id);
    /** ReadOnlyCandidate: parses a public share id/link and returns the target item with temporary token. */
    Result<Model::CloudItem> share_parse(const std::string &id_or_url, const std::string &password = {});
    /** ReadOnlyCandidate: returns a direct download URL for one item or a zip URL for one directory. */
    Result<Model::CloudDownloadUrl> download_url(const CloudItemRef &item);
    /** ReadOnlyCandidate: returns a zip URL for multiple files/dirs, or direct URL for one file. */
    Result<Model::CloudDownloadUrl> batch_download_url(const std::vector<CloudItemRef> &items, const std::string &name);

    void set_write_operation_gate(WriteOperationGate gate);

    Result<Model::MutationResult> create_dir(const std::string &parent_id, const std::string &name);
    Result<Model::MutationResult> rename_item(const std::string &item_id, const std::string &name);
    Result<Model::MutationResult> move_item(const std::string &item_id, const std::string &dest_parent_id);
    Result<Model::MutationResult> copy_item(const CloudItemRef &item, const std::string &dest_parent_id);
    Result<Model::MutationResult> delete_item(const std::string &item_id);
    Result<Model::MutationResult> delete_recycle_item(const std::string &item_id);
    Result<Model::MutationResult> restore_recycle_item(const std::string &item_id);
    Result<Model::MutationResult> share_item(const CloudShareRequest &share);
    Result<Model::MutationResult> share_update(const std::string &share_id, const CloudShareRequest &share);
    Result<Model::MutationResult> share_delete(const std::string &share_id);
    Result<Model::MutationResult> upload_file(const CloudUploadRequest &request, IUploadSource &source);

    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<std::vector<Model::FeatureRecord>> root_records(CloudRootKind kind);
    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<Model::FeatureRecord> user_root_record();
    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<std::vector<Model::FeatureRecord>> list_records(const CloudListQuery &query);
    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<Model::FeatureRecord> size_record(const CloudListQuery &query);
    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<std::vector<Model::FeatureRecord>> recycle_records();
    /** ReadOnlyCandidate: FeatureRecord projection for CLI consumers. */
    Result<std::vector<Model::FeatureRecord>> share_records();
    Result<std::vector<Model::FeatureRecord>> share_record_records(const std::string &item_id);
    Result<Model::FeatureRecord> parsed_share_record(const std::string &id_or_url, const std::string &password = {});
    Result<Model::FeatureRecord> download_url_record(const CloudItemRef &item);
    Result<Model::FeatureRecord> batch_download_url_record(const std::vector<CloudItemRef> &items, const std::string &name);

private:
    IHttpClient &m_http_client;
    ICookieStore *m_cookie_store = nullptr;
    ICacheStore &m_cache;
    ConnectionMode m_mode;
    ICryptoProvider &m_crypto;
    std::optional<CloudLoginCredentials> m_login_credentials;
    std::optional<std::string> m_token;
    WriteOperationGate m_write_gate = disabled_write_operation("file write");

    Result<std::string> token(bool force_refresh = false);
    Result<HttpResponse> send_json_request(HttpRequest request, const std::string &context, const std::string &extra_token = {});
};

} // namespace UBAANext
