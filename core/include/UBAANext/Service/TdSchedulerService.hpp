#pragma once

#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Protocol/TdClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/TdStore.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace UBAANext {

struct TdScheduleWindow {
    int start_minutes = 0;
    int end_minutes = 0;
};

struct TdSchedulerRunOptions {
    std::int64_t timestamp_ms = 0;
    std::string date;
    std::string now_iso;
    int minutes_since_midnight = -1;
    std::optional<int> wait_minutes;
};

struct TdSchedulerUserResult {
    int index = 0;
    std::string student_id;
    bool success = false;
    bool skipped = false;
    bool remote_request = false;
    std::string status;
    std::string message;
    int completed_rounds = 0;
    std::optional<int> term_count;
    int remaining_seconds = 0;
    Model::Td::UserState state;
};

struct TdSchedulerResult {
    std::string date;
    std::string now_iso;
    bool in_window = false;
    int total = 0;
    int success_count = 0;
    int failure_count = 0;
    int skipped_count = 0;
    std::vector<TdSchedulerUserResult> users;
    std::vector<std::string> log_lines;
};

class TdSchedulerService {
public:
    TdSchedulerService(TdStore &store, Protocol::Td::ITdClient &client);

    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);

    [[nodiscard]] static Result<TdScheduleWindow> parse_window(const std::string &window);
    [[nodiscard]] static Result<bool> in_any_window(int minutes_since_midnight, const std::vector<std::string> &windows);

    /**
     * WriteGated remote mutation boundary: advances the TD scheduler by one tick.
     * Tests must inject a mock ITdClient; production callers must confirm before using a real TD transport.
     */
    [[nodiscard]] Result<TdSchedulerResult> run_once(const TdSchedulerRunOptions &options = {});

    /** WriteGated local state mutation: clears today's TD error states and appends an audit log line. */
    [[nodiscard]] Result<int> clear_today_errors(std::string date = {}, std::string now_iso = {});

private:
    TdStore &m_store;
    Protocol::Td::ITdClient &m_client;
    WriteOperationGate m_write_gate = disabled_write_operation("td scheduler once");
};

[[nodiscard]] std::vector<Model::FeatureRecord> td_scheduler_records(const TdSchedulerResult &result);
[[nodiscard]] std::vector<Model::FeatureRecord> td_state_records(const std::vector<Model::Td::UserState> &states);

} // namespace UBAANext
