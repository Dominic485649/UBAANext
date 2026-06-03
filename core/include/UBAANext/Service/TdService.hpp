#pragma once

#include <UBAANext/Protocol/TdClient.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Storage/TdStore.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace UBAANext {

struct TdRunOnceOptions {
    std::int64_t timestamp_ms = 0;
    std::string date;
};

struct TdUserRunResult {
    int index = 0;
    std::string student_id;
    bool success = false;
    bool skipped = false;
    std::string status;
    std::string message;
    std::optional<int> term_count;
    int completed_rounds = 0;
    Model::Td::UserState state;
};

struct TdRunResult {
    int total = 0;
    int success_count = 0;
    int failure_count = 0;
    int skipped_count = 0;
    std::vector<TdUserRunResult> users;
};

class TdService {
public:
    TdService(TdStore &store, Protocol::Td::ITdClient &client);

    /** WriteGated: installs the explicit confirmation and platform write capability gate. */
    void set_write_operation_gate(WriteOperationGate gate);

    /**
     * WriteGated remote mutation: runs one TD entrance/exit round for stored users through the injected client.
     * Tests must inject a mock ITdClient; production callers must confirm before using a real TD transport.
     */
    [[nodiscard]] Result<TdRunResult> run_once(const TdRunOnceOptions &options = {});

private:
    TdStore &m_store;
    Protocol::Td::ITdClient &m_client;
    WriteOperationGate m_write_gate = disabled_write_operation("td run once");
};

} // namespace UBAANext
