#include <UBAANext/Service/TdService.hpp>

#include <UBAANext/Base/TimeUtils.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <utility>

namespace UBAANext {
namespace {

std::int64_t default_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string default_date_string() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto local = local_time(now);
    const auto year = local.tm_year + 1900;
    const auto month = local.tm_mon + 1;
    const auto day = local.tm_mday;
    if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31) return {};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day;
    return output.str();
}

Result<Protocol::Td::ByteVector> read_photo_bytes(const TdStore &store, const std::string &name) {
    auto path = store.image_path(name);
    if (!path) return make_error(path.error().code, path.error().message);
    std::ifstream input(path.value(), std::ios::binary);
    if (!input) return make_error(ErrorCode::StorageError, "无法读取 TD 图片: " + path->string());
    Protocol::Td::ByteVector bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad()) return make_error(ErrorCode::StorageError, "读取 TD 图片失败: " + path->string());
    return bytes;
}

Model::Td::UserState base_state(const Model::Td::User &user, const std::string &date, std::optional<int> count) {
    Model::Td::UserState state;
    state.student_id = user.student_id;
    state.date = date;
    state.status = "pending";
    state.next_action = "entrance";
    state.completed_rounds = 0;
    state.term_count = count;
    return state;
}

std::optional<int> known_term_count(const Model::Td::User &user, const std::optional<Model::Td::UserState> &state) {
    if (state && state->term_count) return state->term_count;
    return user.cached_term_count;
}

bool reached_limit(std::optional<int> count) {
    return count && *count >= Model::Td::completion_limit;
}

std::string limit_message(const Model::Td::User &user) {
    return "用户 " + user.student_id + " TD 次数已达 " + std::to_string(Model::Td::completion_limit) + "，跳过后续打卡";
}

void fill_summary_counts(TdRunResult &result) {
    result.total = static_cast<int>(result.users.size());
    result.success_count = 0;
    result.failure_count = 0;
    result.skipped_count = 0;
    for (const auto &user : result.users) {
        if (user.skipped) ++result.skipped_count;
        if (user.success) ++result.success_count;
        else ++result.failure_count;
    }
}

TdUserRunResult skipped_user_result(int index,
                                    const Model::Td::User &user,
                                    const std::string &date,
                                    std::optional<int> count) {
    auto state = base_state(user, date, count);
    state.status = "completed";
    state.next_action = "none";
    state.last_error.clear();

    TdUserRunResult result;
    result.index = index;
    result.student_id = user.student_id;
    result.success = true;
    result.skipped = true;
    result.status = state.status;
    result.message = limit_message(user);
    result.term_count = state.term_count;
    result.state = state;
    return result;
}

TdUserRunResult failed_user_result(int index,
                                   const Model::Td::User &user,
                                   const std::string &date,
                                   std::optional<int> count,
                                   int completed_rounds,
                                   std::string next_action,
                                   const Error &error) {
    auto state = base_state(user, date, count);
    state.status = "error";
    state.next_action = std::move(next_action);
    state.completed_rounds = completed_rounds;
    state.last_error = error.message;

    TdUserRunResult result;
    result.index = index;
    result.student_id = user.student_id;
    result.success = false;
    result.skipped = false;
    result.status = state.status;
    result.message = error.message;
    result.term_count = state.term_count;
    result.completed_rounds = state.completed_rounds;
    result.state = state;
    return result;
}

TdUserRunResult completed_user_result(int index,
                                      const Model::Td::User &user,
                                      const std::string &date,
                                      std::optional<int> count,
                                      int completed_rounds,
                                      bool limit_reached) {
    auto state = base_state(user, date, count);
    state.status = "completed";
    state.next_action = "none";
    state.completed_rounds = completed_rounds;
    state.last_error.clear();

    TdUserRunResult result;
    result.index = index;
    result.student_id = user.student_id;
    result.success = true;
    result.skipped = false;
    result.status = state.status;
    result.message = limit_reached ? limit_message(user) : "成功";
    result.term_count = state.term_count;
    result.completed_rounds = state.completed_rounds;
    result.state = state;
    return result;
}

class UserRunner {
public:
    UserRunner(TdStore &store,
               Protocol::Td::ITdClient &client,
               const Model::Td::User &user,
               int index,
               std::string date,
               std::int64_t timestamp_ms,
               std::optional<int> initial_count)
        : m_store(store),
          m_client(client),
          m_user(user),
          m_index(index),
          m_date(std::move(date)),
          m_timestamp_ms(timestamp_ms),
          m_count(initial_count) {}

    Result<TdUserRunResult> run() {
        if (reached_limit(m_count)) return skipped_user_result(m_index, m_user, m_date, m_count);

        auto entrance_photo = read_photo_bytes(m_store, m_user.entrance_image);
        if (!entrance_photo) return failed(entrance_photo.error(), 0, "entrance");
        auto exit_photo = read_photo_bytes(m_store, m_user.exit_image);
        if (!exit_photo) return failed(exit_photo.error(), 0, "entrance");

        const auto rounds = std::max(0, m_user.rounds);
        for (int round = 0; round < rounds; ++round) {
            if (reached_limit(m_count)) return completed_user_result(m_index, m_user, m_date, m_count, m_completed_rounds, true);

            auto entrance = m_client.check(m_user, m_user.entrance_machine_id, m_timestamp_ms);
            if (!entrance) return failed(entrance.error(), round, "entrance");
            if (entrance->count) m_count = entrance->count;
            if (!entrance->success) {
                return failed(make_error(ErrorCode::NetworkError, "入口打卡失败: " + entrance->server_message).error, round, "entrance");
            }

            if (reached_limit(m_count)) return completed_user_result(m_index, m_user, m_date, m_count, round, true);

            auto uploaded_entrance = m_client.upload_photo(m_user.entrance_machine_id, entrance_photo.value(), m_timestamp_ms);
            if (!uploaded_entrance) return failed(uploaded_entrance.error(), round, "entrance");

            auto exit = m_client.check(m_user, m_user.exit_machine_id, m_timestamp_ms);
            if (!exit) return failed(exit.error(), round, "exit");
            if (exit->count) m_count = exit->count;
            if (!exit->success) {
                return failed(make_error(ErrorCode::NetworkError, "出口打卡失败: " + exit->server_message).error, round, "exit");
            }

            auto uploaded_exit = m_client.upload_photo(m_user.exit_machine_id, exit_photo.value(), m_timestamp_ms);
            if (!uploaded_exit) return failed(uploaded_exit.error(), round, "exit");

            m_completed_rounds = round + 1;
        }

        return completed_user_result(m_index, m_user, m_date, m_count, m_completed_rounds, reached_limit(m_count));
    }

private:
    Result<TdUserRunResult> failed(const Error &error, int completed_rounds, std::string next_action) const {
        return failed_user_result(m_index, m_user, m_date, m_count, completed_rounds, std::move(next_action), error);
    }

    TdStore &m_store;
    Protocol::Td::ITdClient &m_client;
    const Model::Td::User &m_user;
    int m_index = 0;
    std::string m_date;
    std::int64_t m_timestamp_ms = 0;
    std::optional<int> m_count;
    int m_completed_rounds = 0;
};

} // namespace

TdService::TdService(TdStore &store, Protocol::Td::ITdClient &client) : m_store(store), m_client(client) {}

void TdService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<TdRunResult> TdService::run_once(const TdRunOnceOptions &options) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    auto initialized = m_store.initialize();
    if (!initialized) return make_error(initialized.error().code, initialized.error().message);

    auto config = m_store.load_config();
    if (!config) return make_error(config.error().code, config.error().message);
    (void)config;

    auto users = m_store.load_users();
    if (!users) return make_error(users.error().code, users.error().message);
    auto states = m_store.load_states();
    if (!states) return make_error(states.error().code, states.error().message);

    const auto timestamp_ms = options.timestamp_ms > 0 ? options.timestamp_ms : default_timestamp_ms();
    const auto date = options.date.empty() ? default_date_string() : options.date;

    TdRunResult result;
    int index = 1;
    for (const auto &user : users.value()) {
        const auto state_it = std::find_if(states->begin(), states->end(), [&](const Model::Td::UserState &state) {
            return state.student_id == user.student_id;
        });
        const std::optional<Model::Td::UserState> previous_state = state_it == states->end() ? std::optional<Model::Td::UserState>{}
                                                                                              : std::optional<Model::Td::UserState>(*state_it);
        UserRunner runner(m_store, m_client, user, index, date, timestamp_ms, known_term_count(user, previous_state));
        auto user_result = runner.run();
        if (!user_result) {
            TdUserRunResult failure;
            failure.index = index;
            failure.student_id = user.student_id;
            failure.success = false;
            failure.skipped = false;
            failure.status = "error";
            failure.message = user_result.error().message;
            failure.state = base_state(user, date, known_term_count(user, previous_state));
            failure.state.status = "error";
            failure.state.last_error = user_result.error().message;
            user_result = failure;
        }

        auto saved_state = m_store.save_state(user_result->state);
        if (!saved_state) {
            user_result->success = false;
            user_result->skipped = false;
            user_result->status = "error";
            user_result->message = saved_state.error().message;
            user_result->state.status = "error";
            user_result->state.last_error = saved_state.error().message;
        }
        result.users.push_back(user_result.value());
        ++index;
    }

    fill_summary_counts(result);
    return result;
}

} // namespace UBAANext
