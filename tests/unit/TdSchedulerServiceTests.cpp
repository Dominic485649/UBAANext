#include <UBAANext/Service/TdSchedulerService.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
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
    auto root = std::filesystem::temp_directory_path() / ("ubaanext-td-scheduler-" + name);
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

std::string read_text(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
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

    um::Result<proto::CheckResponse> check(const td::User &user, int machine_id, std::int64_t timestamp_ms) override {
        calls.push_back({"check", user.student_id, machine_id, timestamp_ms, {}});
        if (check_index >= checks.size()) return um::make_error(um::ErrorCode::NetworkError, "mock check missing");
        const auto response = checks[check_index++];
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
        const auto response = uploads[upload_index++];
        if (response.transport_error) return um::make_error(um::ErrorCode::NetworkError, response.transport_error.value());
        proto::TdRawResponse raw;
        raw.status = "success";
        raw.server_message = response.message;
        return raw;
    }

    um::Result<int> query_count(const td::User &, std::optional<int>, std::int64_t) override {
        return um::make_error(um::ErrorCode::NotImplemented, "mock query_count unused");
    }

    std::vector<CheckPlan> checks;
    std::vector<UploadPlan> uploads;
    std::vector<Call> calls;
    std::size_t check_index = 0;
    std::size_t upload_index = 0;
};

struct SchedulerFixture {
    explicit SchedulerFixture(const std::string &name) : root(make_temp_root(name)), store(root) {
        REQUIRE(store.initialize());
        write_text(root / "src-in.jpg", "IN");
        write_text(root / "src-out.jpg", "OUT");
        REQUIRE(store.add_image(root / "src-in.jpg", "in.jpg"));
        REQUIRE(store.add_image(root / "src-out.jpg", "out.jpg"));
    }

    td::User add_user(const std::string &student_id,
                      std::optional<int> cached_count = std::nullopt,
                      int rounds = 2,
                      int wait_min = 5,
                      int wait_max = 5) {
        auto user = td::make_user(student_id, "", 8, 11, "in.jpg", "out.jpg", rounds, wait_min, wait_max, cached_count);
        REQUIRE(user);
        REQUIRE(store.save_user(user.value()));
        return user.value();
    }

    um::TdSchedulerResult run_once(MockTdClient &client,
                                   const um::TdSchedulerRunOptions &options = in_window_options()) {
        um::TdSchedulerService service(store, client);
        service.set_write_operation_gate({true, true, "td scheduler once"});
        auto result = service.run_once(options);
        REQUIRE(result);
        return result.value();
    }

    static um::TdSchedulerRunOptions in_window_options() {
        um::TdSchedulerRunOptions options;
        options.timestamp_ms = 1717401600123;
        options.date = "2026-06-03";
        options.now_iso = "2026-06-03T08:00:00";
        options.minutes_since_midnight = 8 * 60;
        return options;
    }

    std::filesystem::path log_path(const std::string &date = "2026-06-03") const {
        return store.paths().logs_dir / (date + ".log");
    }

    std::filesystem::path root;
    um::TdStore store;
};

} // namespace

TEST_CASE("TdSchedulerService 解析时间窗口并判断当前时间", "[Td][Scheduler]") {
    const auto window = um::TdSchedulerService::parse_window("07:30-10:00");
    REQUIRE(window);
    CHECK(window->start_minutes == 7 * 60 + 30);
    CHECK(window->end_minutes == 10 * 60);

    const auto in_window = um::TdSchedulerService::in_any_window(10 * 60, {"07:30-10:00"});
    REQUIRE(in_window);
    CHECK(*in_window);

    const auto out_window = um::TdSchedulerService::in_any_window(10 * 60 + 1, {"07:30-10:00"});
    REQUIRE(out_window);
    CHECK_FALSE(*out_window);

    const auto reversed = um::TdSchedulerService::parse_window("22:00-07:00");
    REQUIRE_FALSE(reversed);
    CHECK(reversed.error().code == um::ErrorCode::InvalidArgument);

    const auto invalid_minutes = um::TdSchedulerService::in_any_window(24 * 60, {"07:30-10:00"});
    REQUIRE_FALSE(invalid_minutes);
    CHECK(invalid_minutes.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TdSchedulerService run_once 需要写操作确认", "[Td][Scheduler]") {
    SchedulerFixture fixture("gate");
    fixture.add_user("2023123456");
    MockTdClient client;
    um::TdSchedulerService service(fixture.store, client);

    const auto denied = service.run_once(SchedulerFixture::in_window_options());

    REQUIRE_FALSE(denied);
    CHECK(denied.error().code == um::ErrorCode::InvalidArgument);
    CHECK(client.calls.empty());
}

TEST_CASE("TdSchedulerService 空用户只记录轮询心跳", "[Td][Scheduler]") {
    SchedulerFixture fixture("empty");
    MockTdClient client;

    const auto result = fixture.run_once(client);

    CHECK(result.total == 0);
    CHECK(result.success_count == 0);
    CHECK(result.failure_count == 0);
    CHECK(result.skipped_count == 0);
    CHECK(result.users.empty());
    CHECK(client.calls.empty());
    REQUIRE(std::filesystem::exists(fixture.log_path()));
    CHECK(read_text(fixture.log_path()).find("当前没有用户") != std::string::npos);
}

TEST_CASE("TdSchedulerService 窗口外不发送远程请求并作废到期动作", "[Td][Scheduler]") {
    SchedulerFixture fixture("outside-window");
    fixture.add_user("2023123456");
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-03";
    state.status = "waiting";
    state.next_action = "exit";
    state.next_run_at = "2026-06-03T08:00:00";
    state.last_message = "等待出口";
    REQUIRE(fixture.store.save_state(state));
    MockTdClient client;
    auto options = SchedulerFixture::in_window_options();
    options.now_iso = "2026-06-03T22:00:00";
    options.minutes_since_midnight = 22 * 60;

    const auto result = fixture.run_once(client, options);

    REQUIRE(result.users.size() == 1);
    CHECK_FALSE(result.in_window);
    CHECK(client.calls.empty());
    CHECK(result.users[0].remote_request == false);
    CHECK(result.users[0].status == "pending");
    CHECK(result.users[0].message.find("合法时间") != std::string::npos);

    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "pending");
    CHECK((*saved)->next_action == "entrance");
    CHECK((*saved)->next_run_at.empty());
    CHECK((*saved)->last_message.find("合法时间") != std::string::npos);
}

TEST_CASE("TdSchedulerService 执行入口动作后进入等待出口状态", "[Td][Scheduler]") {
    SchedulerFixture fixture("entrance");
    fixture.add_user("2023123456", std::nullopt, 2, 5, 5);
    MockTdClient client;
    client.checks = {{true, "入口成功", 20, std::nullopt}};
    client.uploads = {{"ok"}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.total == 1);
    CHECK(result.success_count == 1);
    CHECK(result.failure_count == 0);
    CHECK(result.users[0].success);
    CHECK(result.users[0].remote_request);
    CHECK(result.users[0].status == "waiting");
    CHECK(result.users[0].term_count == 20);
    CHECK(result.users[0].remaining_seconds == 5 * 60);
    REQUIRE(client.calls.size() == 2);
    CHECK(client.calls[0].kind == "check");
    CHECK(client.calls[0].machine_id == 8);
    CHECK(client.calls[0].timestamp_ms == 1717401600123);
    CHECK(client.calls[1].kind == "upload");
    CHECK(client.calls[1].machine_id == 8);
    const proto::ByteVector entrance_photo{'I', 'N'};
    CHECK(client.calls[1].photo == entrance_photo);

    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "waiting");
    CHECK((*saved)->next_action == "exit");
    CHECK((*saved)->next_run_at == "2026-06-03T08:05:00");
    CHECK((*saved)->term_count == 20);
    CHECK((*saved)->last_message.find("入口完成") != std::string::npos);
    CHECK(read_text(fixture.log_path()).find("入口完成") != std::string::npos);
}

TEST_CASE("TdSchedulerService 等待未到期时不发送远程请求", "[Td][Scheduler]") {
    SchedulerFixture fixture("waiting-not-due");
    fixture.add_user("2023123456");
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-03";
    state.status = "waiting";
    state.next_action = "exit";
    state.term_count = 20;
    state.next_run_at = "2026-06-03T08:30:00";
    state.last_message = "等待出口";
    REQUIRE(fixture.store.save_state(state));
    MockTdClient client;

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(client.calls.empty());
    CHECK(result.users[0].status == "waiting");
    CHECK(result.users[0].remaining_seconds == 30 * 60);
    CHECK(result.users[0].message == "等待 1800 秒");
}

TEST_CASE("TdSchedulerService 等待到期时执行出口动作并安排下一轮入口", "[Td][Scheduler]") {
    SchedulerFixture fixture("exit-due");
    fixture.add_user("2023123456", std::nullopt, 2, 5, 5);
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-03";
    state.status = "waiting";
    state.next_action = "exit";
    state.completed_rounds = 0;
    state.term_count = 20;
    state.next_run_at = "2026-06-03T08:00:00";
    state.last_message = "等待出口";
    REQUIRE(fixture.store.save_state(state));
    MockTdClient client;
    client.checks = {{true, "出口成功", 21, std::nullopt}};
    client.uploads = {{"ok"}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.users[0].success);
    CHECK(result.users[0].status == "waiting");
    CHECK(result.users[0].completed_rounds == 1);
    CHECK(result.users[0].term_count == 21);
    REQUIRE(client.calls.size() == 2);
    CHECK(client.calls[0].kind == "check");
    CHECK(client.calls[0].machine_id == 11);
    CHECK(client.calls[1].kind == "upload");
    CHECK(client.calls[1].machine_id == 11);
    const proto::ByteVector exit_photo{'O', 'U', 'T'};
    CHECK(client.calls[1].photo == exit_photo);

    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "waiting");
    CHECK((*saved)->next_action == "entrance");
    CHECK((*saved)->next_run_at == "2026-06-03T08:05:00");
    CHECK((*saved)->completed_rounds == 1);
}

TEST_CASE("TdSchedulerService 最后一轮出口后标记完成", "[Td][Scheduler]") {
    SchedulerFixture fixture("exit-complete");
    fixture.add_user("2023123456", std::nullopt, 1, 5, 5);
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-03";
    state.status = "waiting";
    state.next_action = "exit";
    state.completed_rounds = 0;
    state.term_count = 20;
    state.next_run_at = "2026-06-03T08:00:00";
    REQUIRE(fixture.store.save_state(state));
    MockTdClient client;
    client.checks = {{true, "出口成功", 21, std::nullopt}};
    client.uploads = {{"ok"}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.users[0].success);
    CHECK(result.users[0].status == "completed");
    CHECK(result.users[0].completed_rounds == 1);
    CHECK(result.users[0].state.next_action == "none");

    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "completed");
    CHECK((*saved)->next_action == "none");
    CHECK((*saved)->completed_rounds == 1);
}

TEST_CASE("TdSchedulerService error 状态保持到手动清除", "[Td][Scheduler]") {
    SchedulerFixture fixture("error-hold");
    fixture.add_user("2023123456");
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-03";
    state.status = "error";
    state.next_action = "entrance";
    state.last_error = "入口失败";
    state.last_message = "请求失败";
    REQUIRE(fixture.store.save_state(state));
    MockTdClient client;

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(client.calls.empty());
    CHECK(result.failure_count == 1);
    CHECK_FALSE(result.users[0].success);
    CHECK(result.users[0].status == "error");
    CHECK(result.users[0].message == "入口失败");
}

TEST_CASE("TdSchedulerService clear_today_errors 需要写操作确认", "[Td][Scheduler]") {
    SchedulerFixture fixture("clear-errors-gate");
    td::UserState today;
    today.student_id = "2023123456";
    today.date = "2026-06-03";
    today.status = "error";
    today.next_action = "exit";
    today.last_error = "失败";
    REQUIRE(fixture.store.save_state(today));
    MockTdClient client;
    um::TdSchedulerService service(fixture.store, client);

    const auto denied = service.clear_today_errors("2026-06-03", "2026-06-03T09:00:00");

    REQUIRE_FALSE(denied);
    CHECK(denied.error().code == um::ErrorCode::InvalidArgument);
    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "error");
}

TEST_CASE("TdSchedulerService clear_today_errors 只清理今日错误", "[Td][Scheduler]") {
    SchedulerFixture fixture("clear-errors");
    td::UserState today;
    today.student_id = "2023123456";
    today.date = "2026-06-03";
    today.status = "error";
    today.next_action = "exit";
    today.next_run_at = "2026-06-03T08:30:00";
    today.last_error = "失败";
    td::UserState old = today;
    old.student_id = "2023123457";
    old.date = "2026-06-02";
    REQUIRE(fixture.store.save_states({today, old}));
    MockTdClient client;
    um::TdSchedulerService service(fixture.store, client);
    service.set_write_operation_gate({true, true, "td scheduler clear-errors"});

    const auto changed = service.clear_today_errors("2026-06-03", "2026-06-03T09:00:00");

    REQUIRE(changed);
    CHECK(*changed == 1);
    const auto today_saved = fixture.store.load_state("2023123456");
    REQUIRE(today_saved);
    REQUIRE(today_saved->has_value());
    CHECK((*today_saved)->status == "pending");
    CHECK((*today_saved)->next_action == "entrance");
    CHECK((*today_saved)->next_run_at.empty());
    CHECK((*today_saved)->last_error.empty());
    CHECK((*today_saved)->last_message.find("错误已清除") != std::string::npos);
    const auto old_saved = fixture.store.load_state("2023123457");
    REQUIRE(old_saved);
    REQUIRE(old_saved->has_value());
    CHECK((*old_saved)->status == "error");
    CHECK(read_text(fixture.log_path()).find("清除 TD 今日错误状态 1 条") != std::string::npos);
}

TEST_CASE("TdSchedulerService 达到 32 次时跳过远程请求", "[Td][Scheduler]") {
    SchedulerFixture fixture("limit-skip");
    fixture.add_user("2023123456", td::completion_limit);
    MockTdClient client;

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(client.calls.empty());
    CHECK(result.users[0].skipped);
    CHECK(result.users[0].status == "completed");
    CHECK(result.users[0].term_count == td::completion_limit);
    CHECK(result.users[0].state.next_action == "none");
}

TEST_CASE("TdSchedulerService 入口返回非法时间时作废本轮且不上传图片", "[Td][Scheduler]") {
    SchedulerFixture fixture("illegal-time");
    fixture.add_user("2023123456");
    MockTdClient client;
    client.checks = {{false, "非法时间", 20, std::nullopt}};

    const auto result = fixture.run_once(client);

    REQUIRE(result.users.size() == 1);
    CHECK(result.users[0].success);
    CHECK(result.users[0].status == "pending");
    CHECK(result.users[0].message.find("非法时间") != std::string::npos);
    REQUIRE(client.calls.size() == 1);
    CHECK(client.calls[0].kind == "check");

    const auto saved = fixture.store.load_state("2023123456");
    REQUIRE(saved);
    REQUIRE(saved->has_value());
    CHECK((*saved)->status == "pending");
    CHECK((*saved)->next_action == "entrance");
    CHECK((*saved)->next_run_at.empty());
    CHECK((*saved)->last_message.find("非法时间") != std::string::npos);
}
