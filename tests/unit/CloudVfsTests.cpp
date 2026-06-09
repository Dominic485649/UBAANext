#include <UBAANext/CloudVfs/CloudVfs.hpp>
#include <UBAANext/Net/CookieJar.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace um = UBAANext;
namespace cvfs = UBAANext::CloudVfs;

namespace {

class CloudVfsHttpClient final : public um::IHttpClient {
public:
    um::Result<um::HttpResponse> send(const um::HttpRequest &request) override {
        requests.push_back(request);
        if (fail_request_number != 0 && requests.size() == fail_request_number) {
            return um::make_error(um::ErrorCode::NetworkError, "token=secret-token&path=C:/secret/file.txt");
        }
        if (network_error_next) {
            network_error_next = false;
            return um::make_error(um::ErrorCode::NetworkError, "token=secret-token&path=C:/secret/file.txt");
        }
        if (next < responses.size()) return responses[next++];
        return um::make_error(um::ErrorCode::NetworkError, "unexpected request");
    }

    void push(int status, std::string body, std::unordered_map<std::string, std::string> headers = {}) {
        um::HttpResponse response;
        response.status_code = status;
        response.body = std::move(body);
        response.headers = std::move(headers);
        responses.push_back(std::move(response));
    }

    std::vector<um::HttpRequest> requests;
    std::vector<um::HttpResponse> responses;
    std::size_t next = 0;
    std::size_t fail_request_number = 0;
    bool network_error_next = false;
};

class CloudVfsCookieStore final : public um::ICookieStore {
public:
    um::Result<um::CookieJar> load() override { return jar; }
    um::Result<void> save(const um::CookieJar &cookies) override {
        jar = cookies;
        return {};
    }
    um::Result<void> save_current() override { return {}; }
    um::Result<void> clear() override {
        jar.clear();
        return {};
    }
    const um::CookieJar *current() const override { return &jar; }

    um::CookieJar jar;
};

class MemoryUploadSource final : public um::IUploadSource {
public:
    MemoryUploadSource(std::string name, std::string data)
        : m_name(std::move(name)), m_data(std::move(data)) {}

    std::string name() const override { return m_name; }
    std::string content_type() const override { return "application/octet-stream"; }
    um::Result<std::uint64_t> size() override { return static_cast<std::uint64_t>(m_data.size()); }
    um::Result<void> rewind() override {
        m_offset = 0;
        return {};
    }
    um::Result<std::size_t> read(unsigned char *buffer, std::size_t max_bytes) override {
        const auto remaining = m_data.size() - m_offset;
        const auto count = std::min(max_bytes, remaining);
        std::copy_n(reinterpret_cast<const unsigned char *>(m_data.data() + m_offset), count, buffer);
        m_offset += count;
        return count;
    }

private:
    std::string m_name;
    std::string m_data;
    std::size_t m_offset = 0;
};

um::Model::CloudItem root_item() {
    um::Model::CloudItem item;
    item.id = "root";
    item.name = "Cloud";
    item.type = "doc_lib";
    return item;
}

std::string list_body(std::string file_name = "a.txt") {
    return R"JSON({"success":true,"data":{"dirs":[{"id":"dir-1","name":"Docs","size":"-1","parentId":"root","revision":"d1"}],"files":[{"docid":"file-1","name":")JSON" +
           file_name +
           R"JSON(","size":"5","parentId":"root","revision":"r1"}]}})JSON";
}

std::string bytes_to_string(const std::vector<unsigned char> &bytes) {
    return std::string(bytes.begin(), bytes.end());
}

std::vector<unsigned char> string_to_bytes(const std::string &text) {
    return std::vector<unsigned char>(text.begin(), text.end());
}

void allow_service_writes(um::CloudService &service) {
    um::WriteOperationGate service_gate;
    service_gate.confirmed = true;
    service_gate.allow_write_operations = true;
    service_gate.operation = "cloud vfs upload";
    service.set_write_operation_gate(service_gate);
}

} // namespace

TEST_CASE("CloudVfs caches metadata and maps paths to docids", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, list_body("b.txt"));
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));

    auto children = vfs.list("/");
    REQUIRE(children);
    CHECK(children->size() == 2);
    CHECK(vfs.lookup("/Docs")->docid == "dir-1");
    CHECK(vfs.lookup("\\a.txt")->docid == "file-1");
    CHECK(http.requests.size() == 1);

    auto cached = vfs.list("/");
    REQUIRE(cached);
    CHECK(http.requests.size() == 1);

    auto refreshed = vfs.refresh("/");
    REQUIRE(refreshed);
    CHECK(http.requests.size() == 2);
    CHECK(vfs.lookup("/b.txt")->docid == "file-1");
    CHECK_FALSE(vfs.lookup("/a.txt"));
}

TEST_CASE("CloudVfs range reads use content cache", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"url":"https://download.example/file?token=secret-token"}})JSON");
    http.push(206, "hello");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));

    auto first = vfs.read("/a.txt", 0, 5);
    REQUIRE(first);
    CHECK(bytes_to_string(*first) == "hello");
    REQUIRE(http.requests.size() == 3);
    CHECK(http.requests[2].headers.at("Range") == "bytes=0-4");

    auto second = vfs.read("/a.txt", 0, 5);
    REQUIRE(second);
    CHECK(bytes_to_string(*second) == "hello");
    CHECK(http.requests.size() == 3);
}

TEST_CASE("CloudVfs range reads clamp oversized responses before caching", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"url":"https://download.example/file?token=secret-token"}})JSON");
    http.push(206, "helloextra");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));

    auto first = vfs.read("/a.txt", 0, 5);
    REQUIRE(first);
    CHECK(bytes_to_string(*first) == "hello");
    CHECK(content_cache.used_bytes() == 5);

    auto second = vfs.read("/a.txt", 1, 4);
    REQUIRE(second);
    CHECK(bytes_to_string(*second) == "ello");
    CHECK(http.requests.size() == 3);
}

TEST_CASE("CloudVfs range read failures are redacted", "[cloud-vfs][security]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"url":"https://download.example/file?token=secret-token"}})JSON");
    http.fail_request_number = 3;
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));

    auto failed = vfs.read("/a.txt", 0, 5);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().message.find("secret-token") == std::string::npos);
    CHECK(failed.error().message.find("C:/secret") == std::string::npos);
    CHECK(failed.error().message.find("[REDACTED]") != std::string::npos);
}

TEST_CASE("CloudVfs upload queue enforces authorization and conflict policy", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"name":"a (1).txt"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    REQUIRE(vfs.set_root(root_item()));

    auto blocked = vfs.enqueue_upload("/", "new.txt", std::make_shared<MemoryUploadSource>("new.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().message.find("未授权") != std::string::npos);

    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    auto conflict = vfs.enqueue_upload("/", "a.txt", std::make_shared<MemoryUploadSource>("a.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE_FALSE(conflict);
    CHECK(conflict.error().message.find("同名") != std::string::npos);

    auto suggested = vfs.enqueue_upload("/", "a.txt", std::make_shared<MemoryUploadSource>("a.txt", "hello"), cvfs::CloudVfsConflictPolicy::UseSuggestedName);
    REQUIRE(suggested);
    CHECK(suggested->name == "a (1).txt");
}

TEST_CASE("CloudVfs upload queue reuses CloudService upload implementation", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-new","rev":"rev-new"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-new"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto queued = vfs.enqueue_upload("/", "new.txt", std::make_shared<MemoryUploadSource>("new.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(queued);
    auto processed = vfs.process_next_upload();
    REQUIRE(processed);
    CHECK(processed->status == cvfs::CloudVfsTaskStatus::Succeeded);
    CHECK(processed->result_docid == "file-new");
    REQUIRE(http.requests.size() == 5);
    CHECK(http.requests[3].method == um::HttpMethod::Put);
    CHECK(http.requests[3].body == "hello");
}

TEST_CASE("CloudVfs refresh removes stale paths and cached content", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body("a.txt"));
    http.push(200, R"JSON({"success":true,"data":{"url":"https://download.example/file?token=secret-token"}})JSON");
    http.push(206, "hello");
    http.push(200, list_body("b.txt"));
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));
    REQUIRE(vfs.read("/a.txt", 0, 5));
    CHECK(content_cache.used_bytes() == 5);

    auto refreshed = vfs.refresh("/");
    REQUIRE(refreshed);
    REQUIRE_FALSE(vfs.lookup("/a.txt"));
    REQUIRE(vfs.lookup("/b.txt"));
    CHECK(content_cache.used_bytes() == 0);
}

TEST_CASE("CloudVfs content cache can be cleared only through explicit authorization", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    CloudVfsCookieStore cookies;
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    content_cache.put_range("file-1", "rev-1", 0, {'h', 'i'});
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));

    auto blocked = vfs.clear_content_cache();
    REQUIRE_FALSE(blocked);
    CHECK(content_cache.used_bytes() == 2);

    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    auto cleared = vfs.clear_content_cache();
    REQUIRE(cleared);
    CHECK(content_cache.used_bytes() == 0);
}

TEST_CASE("CloudVfs upload failures are redacted and remain retryable", "[cloud-vfs][security]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.fail_request_number = 3;
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto queued = vfs.enqueue_upload("/", "new.txt", std::make_shared<MemoryUploadSource>("new.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(queued);
    auto failed = vfs.process_next_upload();
    REQUIRE(failed);
    CHECK(failed->status == cvfs::CloudVfsTaskStatus::Failed);
    CHECK(failed->attempts == 1);
    CHECK(failed->error_message.find("secret-token") == std::string::npos);
    CHECK(failed->error_message.find("C:/secret") == std::string::npos);
    CHECK(failed->error_message.find("[REDACTED]") != std::string::npos);

    auto tasks = vfs.tasks();
    REQUIRE(tasks.size() == 1);
    CHECK(tasks[0].status == cvfs::CloudVfsTaskStatus::Failed);

    auto marked_retry = vfs.retry_upload(queued->id);
    REQUIRE(marked_retry);
    CHECK(marked_retry->status == cvfs::CloudVfsTaskStatus::Pending);

    auto retry = vfs.process_next_upload();
    REQUIRE(retry);
    CHECK(retry->status == cvfs::CloudVfsTaskStatus::Failed);
    CHECK(retry->attempts == 2);
}

TEST_CASE("CloudVfs range reads clamp to file size", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"url":"https://download.example/file?token=secret-token"}})JSON");
    http.push(206, "lo");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfs vfs(service, content_cache);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));

    auto clipped = vfs.read("/a.txt", 3, 10);
    REQUIRE(clipped);
    CHECK(bytes_to_string(*clipped) == "lo");
    REQUIRE(http.requests.size() == 3);
    CHECK(http.requests[2].headers.at("Range") == "bytes=3-4");

    auto eof = vfs.read("/a.txt", 5, 10);
    REQUIRE(eof);
    CHECK(eof->empty());
    CHECK(http.requests.size() == 3);
}

TEST_CASE("CloudVfs temp write flush and close enqueue upload", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-new","rev":"rev-new"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-new"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto handle = vfs.open_temp_write("/", "new.txt", cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(handle);
    REQUIRE(vfs.write(*handle, 0, string_to_bytes("he")));
    REQUIRE(vfs.write(*handle, 2, string_to_bytes("llo")));
    auto queued = vfs.flush(*handle);
    REQUIRE(queued);
    CHECK(queued->status == cvfs::CloudVfsTaskStatus::Pending);
    auto duplicate_flush = vfs.flush(*handle);
    REQUIRE(duplicate_flush);
    CHECK(duplicate_flush->id == queued->id);
    auto closed = vfs.close(*handle);
    REQUIRE(closed);
    CHECK_FALSE(handle->writable);

    auto processed = vfs.process_next_upload();
    REQUIRE(processed);
    CHECK(processed->status == cvfs::CloudVfsTaskStatus::Succeeded);
    REQUIRE(http.requests.size() == 5);
    CHECK(http.requests[3].body == "hello");
}

TEST_CASE("CloudVfs temp write keeps suggested name stable after flush", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{"name":"a (1).txt"}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-new","rev":"rev-new"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-new"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto handle = vfs.open_temp_write("/", "a.txt", cvfs::CloudVfsConflictPolicy::UseSuggestedName);
    REQUIRE(handle);
    CHECK(handle->path == "/a (1).txt");
    REQUIRE(vfs.write(*handle, 0, string_to_bytes("hello")));
    auto queued = vfs.flush(*handle);
    REQUIRE(queued);
    CHECK(queued->name == "a (1).txt");
    CHECK(queued->path == "/a (1).txt");

    auto processed = vfs.process_next_upload();
    REQUIRE(processed);
    CHECK(processed->status == cvfs::CloudVfsTaskStatus::Succeeded);
    CHECK(processed->name == "a (1).txt");
    REQUIRE(http.requests.size() == 6);
    CHECK(http.requests[4].body == "hello");
}

TEST_CASE("CloudVfs content cache rejects overflowed range hits", "[cloud-vfs]") {
    cvfs::MemoryCloudVfsContentCache content_cache;
    content_cache.put_range("file-1", "rev-1", std::numeric_limits<std::uint64_t>::max() - 1, {'a', 'b'});

    auto hit = content_cache.get_range("file-1", "rev-1", std::numeric_limits<std::uint64_t>::max() - 1, 2);
    REQUIRE(hit);
    CHECK(bytes_to_string(*hit) == "ab");

    auto overflow = content_cache.get_range("file-1", "rev-1", std::numeric_limits<std::uint64_t>::max() - 1,
                                            std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE(overflow);
}

TEST_CASE("CloudVfs temp write rejects offsets outside memory address space", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto handle = vfs.open_temp_write("/", "new.txt", cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(handle);
    auto failed = vfs.write(*handle, static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()), string_to_bytes("x"));
    REQUIRE_FALSE(failed);
    CHECK(failed.error().message.find("上限") != std::string::npos);
}

TEST_CASE("CloudVfs failed upload tasks can be cancelled before cleanup", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.fail_request_number = 3;
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto queued = vfs.enqueue_upload("/", "new.txt", std::make_shared<MemoryUploadSource>("new.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(queued);
    auto failed = vfs.process_next_upload();
    REQUIRE(failed);
    CHECK(failed->status == cvfs::CloudVfsTaskStatus::Failed);

    auto cancelled = vfs.cancel_upload(queued->id);
    REQUIRE(cancelled);
    CHECK(cancelled->status == cvfs::CloudVfsTaskStatus::Cancelled);
    CHECK(vfs.cleanup_tasks() == 1);
    CHECK(vfs.tasks().empty());
}

TEST_CASE("CloudVfs successful upload invalidates cached content at replaced path", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-new","rev":"rev-new"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-new"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));
    REQUIRE(vfs.list("/"));
    content_cache.put_range("file-1", "r1", 0, {'o', 'l', 'd'});

    auto queued = vfs.enqueue_upload("/", "a.txt", std::make_shared<MemoryUploadSource>("a.txt", "new"), cvfs::CloudVfsConflictPolicy::Overwrite);
    REQUIRE(queued);
    auto processed = vfs.process_next_upload();
    REQUIRE(processed);
    CHECK(processed->status == cvfs::CloudVfsTaskStatus::Succeeded);
    CHECK(content_cache.used_bytes() == 0);
}
TEST_CASE("CloudVfs upload tasks can be cancelled and cleaned up", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, R"JSON({"success":true,"data":{"dirs":[],"files":[]}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto queued = vfs.enqueue_upload("/", "new.txt", std::make_shared<MemoryUploadSource>("new.txt", "hello"), cvfs::CloudVfsConflictPolicy::Fail);
    REQUIRE(queued);
    auto cancelled = vfs.cancel_upload(queued->id);
    REQUIRE(cancelled);
    CHECK(cancelled->status == cvfs::CloudVfsTaskStatus::Cancelled);
    REQUIRE_FALSE(vfs.process_next_upload());
    CHECK(vfs.cleanup_tasks() == 1);
    CHECK(vfs.tasks().empty());
}

TEST_CASE("CloudVfs overwrite deletes stale remote item before upload", "[cloud-vfs]") {
    CloudVfsHttpClient http;
    http.push(200, list_body());
    http.push(200, list_body());
    http.push(200, R"JSON({"success":true,"data":{}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"match":false}})JSON");
    http.push(200, R"JSON({"success":true,"data":{"authrequest":["PUT","https://upload.example/small","Content-Type: application/octet-stream"],"docid":"file-new","rev":"rev-new"}})JSON");
    http.push(200, "");
    http.push(200, R"JSON({"success":true,"data":{"docid":"file-new"}})JSON");
    CloudVfsCookieStore cookies;
    cookies.jar.set_cookie("bhpan.buaa.edu.cn", "client.oauth2_token", "cloud-token");
    um::MemoryCacheStore cache;
    um::CloudService service(http, &cookies, cache, um::ConnectionMode::Direct);
    allow_service_writes(service);

    cvfs::MemoryCloudVfsContentCache content_cache;
    cvfs::CloudVfsConfig config;
    config.read_only = false;
    cvfs::CloudVfs vfs(service, content_cache, config);
    cvfs::AllowAllCloudVfsWriteGate gate;
    vfs.set_write_gate(&gate);
    REQUIRE(vfs.set_root(root_item()));

    auto queued = vfs.enqueue_upload("/", "a.txt", std::make_shared<MemoryUploadSource>("a.txt", "hello"), cvfs::CloudVfsConflictPolicy::Overwrite);
    REQUIRE(queued);
    auto processed = vfs.process_next_upload();
    REQUIRE(processed);
    CHECK(processed->status == cvfs::CloudVfsTaskStatus::Succeeded);
    REQUIRE(http.requests.size() == 7);
    CHECK(http.requests[2].body.find("file-1") != std::string::npos);
    CHECK(http.requests[5].body == "hello");
}
