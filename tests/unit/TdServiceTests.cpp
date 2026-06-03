#include <UBAANext/Service/TdService.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace proto = UBAANext::Protocol::Td;
namespace td = UBAANext::Model::Td;
namespace um = UBAANext;

namespace {

std::filesystem::path make_temp_root(const std::string &name) {
    auto root = std::filesystem::temp_directory_path() / ("ubaanext-td-service-" + name);
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    return root;
}

void write_text(const std::filesystem::path &path, const std::string &text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

class MockTdClient : public proto::ITdClient {
public:
    struct Call {
        std::string kind;
        std::string student_id;
        int machine_id = 0;
        std::int64_t timestamp_ms = 0;
        proto::ByteVector photo;
    };

    um::Result<proto::CheckResponse> check(const td::User &user, int machine_id, std::int64_t timestamp_ms) override {
        calls.push_back({"check", user.student_id, machine_id, timestamp_ms, {}});
        if (check_index >= checks.size()) return um::make_error(um::ErrorCode::NetworkError, "mock check missing");
        auto response = checks[check_index++];
        if (response.transport_error) return um::make_error(um::ErrorCode::NetworkError, response.transport_error.value());
        proto::CheckResponse check_response;
        check_response.success = response.success;
        check_response.server_message = response.message;
        check_response.count = response.count;
        return check_response;
    }

    um::Result<proto::TdRawResponse> upload_photo(int machine_id,
                                                  const proto::ByteVector &photo,
                                                  std::int64_t timestamp_ms) override {
        calls.push_back({"upload", {}, machine_id, timestamp_ms, photo});
        if (upload_index >= uploads.size()) return um::make_error(um::ErrorCode::NetworkError, "mock upload missing");
        auto response = uploads[upload_index++];
        if (response.transport_error) return um::make_error(um::ErrorCode::NetworkError, response.transport_error.value());
        proto::TdRawResponse raw;
        raw.status = "success";
        raw.server_message = response.message;
        return raw;
    }

    um::Result<int> query_count(const td::User &, std::optional<int>, std::int64_t) override {
        return um::make_error(um::ErrorCode::NotImplemented, "mock query_count unused");
    }

    struct CheckPlan {
        bool success = true;
        std::string message = "成功";
        std::optional<int> count = std::nullopt;
        std::optional<std::string> transport_error = std::nullopt;
    };

    struct UploadPlan {
        std::string message = "ok";
        std::optional<std::string> transport_error = std::nullopt;
    };

    std::vector<CheckPlan> checks;
    std::vector<UploadPlan> uploads;
    std::vector<Call> calls;
    std::size_t check_index = 0;
    std::size_t upload_index = 0;
};

struct ServiceFixture {
    explicit ServiceFixture(const std::string &name) : root(make_temp_root(name)), store(root) {
        REQUIRE(store.initialize());
        write_text(root / "src-in.jpg", "IN");
        write_text(root / "src-out.jpg", "OUT");
        REQUIRE(store.add_image(root / "src-in.jpg", "in.jpg"));
        REQUIRE(store.add_image(root / "src-out.jpg", "out.jpg"));
    }

    td::User add_user(const std::string &student_id,
                      std::optional<int> cached_count = std::nullopt,
                      int rounds = 1) {
        auto user = td::make_user(student_id, "", 8, 11, "in.jpg", "out.jpg", rounds, 0, 0, cached_count);
        REQUIRE(user);
        REQUIRE(store.save_user(user.value()));
        return user.value();
    }

    um::TdRunResult run_once(MockTdClient &client, std::string date = "2026-06-02") {
        um::TdService service(store, client);
        service.set_write_operation_gate({true, true, "td run once"});
        um::TdRunOnceOptions options;
        options.timestamp_ms = 1717200000123;
        options.date = std::move(date);
        auto result = service.run_once(options);
        REQUIRE(result);
        return result.value();
    }

    std::filesystem::path root;
    um::TdStore store;
};

} // namespace

TEST_CASE("TdService run_once 需要写操作确认", "[Td][Service]") {
    ServiceFixture fixture("gate");
    MockTdClient client;
    fixture.add_user("2023123456");
    um::TdService service(fixture.store, client);

    const auto denied = service.run_once({});

    REQUIRE_FALSE(denied);
    CHECK(denied.error().code == um::ErrorCode::InvalidArgument);
    CHECK(client.calls.empty());
}

TEST_CASE("TdService run_once 空用户直接返回空批次", "[Td][Service]") {
    ServiceFixture fixture("empty");
    MockTdClient client;

    const auto result = fixture.run_once(client);

    CHECK(result.total == 0);
    CHECK(result.success_count == 0);
    CHECK(result.failure_count == 0);
    CHECK(result.skipped_count == 0);
    CHECK(result.users.empty());
    CHECK(client.calls.empty());
}

TEST_CASE("TdService run_once 达到 32 次时跳过真实请求并写入状态", "[Td][Service]") {
    ServiceFixture fixture("limit-skip");
    fixture.add_user("2023123456", td::completion_limit);
    MockTdClient client;

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.total == 1);
    CHECK(result.success_count == 1);
    CHECK(result.failure_count == 0);
    CHECK(result.skipped_count == 1);
    CHECK(result.users[0].skipped);
    CHECK(result.users[0].term_count == td::completion_limit);
    CHECK(client.calls.empty());

    const auto state = fixture.store.load_state("2023123456");
    REQUIRE(state);
    REQUIRE(state->has_value());
    CHECK((*state)->status == "completed");
    CHECK((*state)->next_action == "none");
    CHECK((*state)->term_count == td::completion_limit);
}

TEST_CASE("TdService run_once 单用户失败不中断后续用户", "[Td][Service]") {
    ServiceFixture fixture("continue-after-failure");
    fixture.add_user("2023123456");
    fixture.add_user("2023123457");
    MockTdClient client;
    client.checks = {{false, "入口拒绝", 12, std::nullopt}, {true, "入口成功", 13, std::nullopt}, {true, "出口成功", 14, std::nullopt}};
    client.uploads = {{"ok"}, {"ok"}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 2);
    CHECK(result.total == 2);
    CHECK(result.success_count == 1);
    CHECK(result.failure_count == 1);
    CHECK(result.skipped_count == 0);
    CHECK(result.users[0].student_id == "2023123456");
    CHECK_FALSE(result.users[0].success);
    CHECK(result.users[0].status == "error");
    CHECK(result.users[0].state.next_action == "entrance");
    CHECK(result.users[0].term_count == 12);
    CHECK(result.users[1].student_id == "2023123457");
    CHECK(result.users[1].success);
    CHECK(result.users[1].term_count == 14);
    REQUIRE(client.calls.size() == 5);
    CHECK(client.calls[0].kind == "check");
    CHECK(client.calls[0].student_id == "2023123456");
    CHECK(client.calls[1].kind == "check");
    CHECK(client.calls[1].student_id == "2023123457");
    CHECK(client.calls[2].kind == "upload");
    const proto::ByteVector entrance_photo{'I', 'N'};
    const proto::ByteVector exit_photo{'O', 'U', 'T'};
    CHECK(client.calls[2].photo == entrance_photo);
    CHECK(client.calls[3].kind == "check");
    CHECK(client.calls[3].student_id == "2023123457");
    CHECK(client.calls[4].kind == "upload");
    CHECK(client.calls[4].photo == exit_photo);

    const auto failed_state = fixture.store.load_state("2023123456");
    REQUIRE(failed_state);
    REQUIRE(failed_state->has_value());
    CHECK((*failed_state)->status == "error");
    CHECK((*failed_state)->last_error.find("入口打卡失败") != std::string::npos);

    const auto ok_state = fixture.store.load_state("2023123457");
    REQUIRE(ok_state);
    REQUIRE(ok_state->has_value());
    CHECK((*ok_state)->status == "completed");
    CHECK((*ok_state)->completed_rounds == 1);
    CHECK((*ok_state)->term_count == 14);
}

TEST_CASE("TdService run_once 入口返回 32 次后停止后续动作", "[Td][Service]") {
    ServiceFixture fixture("limit-after-entrance");
    fixture.add_user("2023123456");
    MockTdClient client;
    client.checks = {{true, "入口成功", td::completion_limit, std::nullopt}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.users[0].success);
    CHECK_FALSE(result.users[0].skipped);
    CHECK(result.users[0].term_count == td::completion_limit);
    CHECK(result.users[0].completed_rounds == 0);
    REQUIRE(client.calls.size() == 1);
    CHECK(client.calls[0].kind == "check");
    CHECK(client.calls[0].machine_id == 8);

    const auto state = fixture.store.load_state("2023123456");
    REQUIRE(state);
    REQUIRE(state->has_value());
    CHECK((*state)->status == "completed");
    CHECK((*state)->next_action == "none");
    CHECK((*state)->term_count == td::completion_limit);
}

TEST_CASE("TdService run_once 成功执行一轮并写入 completed 状态", "[Td][Service]") {
    ServiceFixture fixture("success");
    fixture.add_user("2023123456");
    MockTdClient client;
    client.checks = {{true, "入口成功", 20, std::nullopt}, {true, "出口成功", 21, std::nullopt}};
    client.uploads = {{"ok"}, {"ok"}};

    const auto result = fixture.run_once(client, "2026-06-03");

    REQUIRE(result.users.size() == 1);
    CHECK(result.total == 1);
    CHECK(result.success_count == 1);
    CHECK(result.failure_count == 0);
    CHECK(result.skipped_count == 0);
    CHECK(result.users[0].student_id == "2023123456");
    CHECK(result.users[0].success);
    CHECK(result.users[0].status == "completed");
    CHECK(result.users[0].term_count == 21);
    CHECK(result.users[0].completed_rounds == 1);
    REQUIRE(client.calls.size() == 4);
    CHECK(client.calls[0].kind == "check");
    CHECK(client.calls[0].machine_id == 8);
    CHECK(client.calls[0].timestamp_ms == 1717200000123);
    CHECK(client.calls[1].kind == "upload");
    CHECK(client.calls[1].machine_id == 8);
    const proto::ByteVector entrance_photo{'I', 'N'};
    const proto::ByteVector exit_photo{'O', 'U', 'T'};
    CHECK(client.calls[1].photo == entrance_photo);
    CHECK(client.calls[2].kind == "check");
    CHECK(client.calls[2].machine_id == 11);
    CHECK(client.calls[3].kind == "upload");
    CHECK(client.calls[3].machine_id == 11);
    CHECK(client.calls[3].photo == exit_photo);

    const auto state = fixture.store.load_state("2023123456");
    REQUIRE(state);
    REQUIRE(state->has_value());
    CHECK((*state)->date == "2026-06-03");
    CHECK((*state)->status == "completed");
    CHECK((*state)->next_action == "none");
    CHECK((*state)->completed_rounds == 1);
    CHECK((*state)->term_count == 21);
    CHECK((*state)->last_error.empty());
}
