#include <UBAANext/Service/TdSchedulerService.hpp>

#include <UBAANext/Base/TimeUtils.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace UBAANext {
namespace {

std::int64_t default_timestamp_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::tm default_local_tm() {
    return local_time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
}

std::string two_digits(int value) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << value;
    return output.str();
}

std::string date_from_tm(const std::tm &time) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << (time.tm_year + 1900) << '-'
           << std::setw(2) << (time.tm_mon + 1) << '-'
           << std::setw(2) << time.tm_mday;
    return output.str();
}

std::string iso_from_tm(const std::tm &time) {
    return date_from_tm(time) + "T" + two_digits(time.tm_hour) + ":" + two_digits(time.tm_min) + ":" + two_digits(time.tm_sec);
}

std::string default_date_string() {
    return date_from_tm(default_local_tm());
}

std::string default_iso_string() {
    return iso_from_tm(default_local_tm());
}

int default_minutes_since_midnight() {
    const auto local = default_local_tm();
    return local.tm_hour * 60 + local.tm_min;
}

std::string trim(std::string value) {
    auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

Result<int> parse_int(std::string_view text, std::string label) {
    if (text.empty()) return make_error(ErrorCode::InvalidArgument, label + " 不能为空");
    int value = 0;
    for (const char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return make_error(ErrorCode::InvalidArgument, label + " 必须是整数");
        value = value * 10 + (ch - '0');
    }
    return value;
}

Result<int> hhmm_to_minutes(const std::string &value) {
    const auto text = trim(value);
    const auto colon = text.find(':');
    if (colon == std::string::npos) return make_error(ErrorCode::InvalidArgument, "非法时间: " + value);
    auto hour = parse_int(std::string_view(text).substr(0, colon), "小时");
    if (!hour) return make_error(hour.error().code, hour.error().message);
    auto minute = parse_int(std::string_view(text).substr(colon + 1), "分钟");
    if (!minute) return make_error(minute.error().code, minute.error().message);
    if (*hour < 0 || *hour > 23 || *minute < 0 || *minute > 59) return make_error(ErrorCode::InvalidArgument, "非法时间: " + value);
    return *hour * 60 + *minute;
}

std::optional<std::tm> parse_iso_tm(const std::string &value) {
    if (value.size() < 16) return std::nullopt;
    std::tm time{};
    std::istringstream input(value.substr(0, 19));
    input >> std::get_time(&time, "%Y-%m-%dT%H:%M:%S");
    if (input.fail()) {
        input.clear();
        input.str(value.substr(0, 16));
        input >> std::get_time(&time, "%Y-%m-%dT%H:%M");
        if (input.fail()) return std::nullopt;
    }
    time.tm_isdst = -1;
    return time;
}

std::optional<std::time_t> parse_iso_time_t(const std::string &value) {
    auto time = parse_iso_tm(value);
    if (!time) return std::nullopt;
    return std::mktime(&*time);
}

std::string add_minutes_to_iso(const std::string &now_iso, int minutes) {
    auto now = parse_iso_time_t(now_iso);
    if (!now) return now_iso;
    const auto due = *now + static_cast<std::time_t>(std::max(0, minutes)) * 60;
    return iso_from_tm(local_time(due));
}

int remaining_seconds(const std::string &now_iso, const std::string &due_iso) {
    auto now = parse_iso_time_t(now_iso);
    auto due = parse_iso_time_t(due_iso);
    if (!now || !due || *due <= *now) return 0;
    return static_cast<int>(*due - *now);
}

bool due_after_now(const std::string &due_iso, const std::string &now_iso) {
    auto remaining = remaining_seconds(now_iso, due_iso);
    return remaining > 0;
}

bool reached_limit(std::optional<int> count) {
    return count && *count >= Model::Td::completion_limit;
}

std::optional<int> known_term_count(const Model::Td::User &user, const Model::Td::UserState &state) {
    if (state.term_count) return state.term_count;
    return user.cached_term_count;
}

std::string count_text(std::optional<int> count) {
    return count ? std::to_string(*count) : std::string{};
}

std::string int_text(int value) {
    return std::to_string(value);
}

Model::Td::UserState new_state(const Model::Td::User &user, const std::string &date, std::optional<int> count) {
    Model::Td::UserState state;
    state.student_id = user.student_id;
    state.date = date;
    state.status = "pending";
    state.next_action = "entrance";
    state.completed_rounds = 0;
    state.term_count = count;
    state.last_message = "等待入口打卡";
    return state;
}

Model::Td::UserState today_state(const Model::Td::User &user,
                                 const std::vector<Model::Td::UserState> &states,
                                 const std::string &date) {
    const auto it = std::find_if(states.begin(), states.end(), [&](const Model::Td::UserState &state) {
        return state.student_id == user.student_id;
    });
    if (it == states.end() || it->date != date) return new_state(user, date, it == states.end() ? user.cached_term_count : known_term_count(user, *it));
    auto state = *it;
    if (state.next_action.empty()) state.next_action = "entrance";
    return state;
}

Result<Protocol::Td::ByteVector> read_photo_bytes(const TdStore &store, const std::string &name) {
    auto path = store.image_path(name);
    if (!path) return make_error(path.error().code, path.error().message);
    std::ifstream input(path.value(), std::ios::binary);
    if (!input) return make_error(ErrorCode::StorageError, "无法读取 TD 图片: " + path->u8string());
    Protocol::Td::ByteVector bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad()) return make_error(ErrorCode::StorageError, "读取 TD 图片失败: " + path->u8string());
    return bytes;
}

std::string limit_message(const Model::Td::User &user) {
    return "用户 " + user.student_id + " TD 次数已达 " + std::to_string(Model::Td::completion_limit) + "，跳过后续打卡";
}

void mark_completed(Model::Td::UserState &state, const Model::Td::User &, std::string message) {
    state.status = "completed";
    state.next_action = "none";
    state.next_run_at.clear();
    state.last_message = std::move(message);
    state.last_error.clear();
}

void mark_error(Model::Td::UserState &state, std::string message) {
    state.status = "error";
    state.last_error = std::move(message);
    state.last_message = "请求失败，等待清除错误后重试";
    state.next_run_at.clear();
}

void discard_current_round(Model::Td::UserState &state, const Model::Td::User &user, const std::string &action_label) {
    const auto round_no = state.completed_rounds + 1;
    state.status = "pending";
    state.next_action = "entrance";
    state.next_run_at.clear();
    state.last_message = "第 " + std::to_string(round_no) + "/" + std::to_string(user.rounds) + " 轮" + action_label + "非法时间，本轮作废";
    state.last_error.clear();
}

TdSchedulerUserResult user_result(int index,
                                  const Model::Td::User &user,
                                  bool success,
                                  bool skipped,
                                  bool remote_request,
                                  std::string message,
                                  const Model::Td::UserState &state,
                                  int remaining = 0) {
    TdSchedulerUserResult result;
    result.index = index;
    result.student_id = user.student_id;
    result.success = success;
    result.skipped = skipped;
    result.remote_request = remote_request;
    result.status = state.status;
    result.message = std::move(message);
    result.completed_rounds = state.completed_rounds;
    result.term_count = state.term_count;
    result.remaining_seconds = remaining;
    result.state = state;
    return result;
}

int wait_minutes_for_user(const Model::Td::User &user, const TdSchedulerRunOptions &options) {
    if (options.wait_minutes) return std::max(0, *options.wait_minutes);
    return std::max(0, user.wait_time_min_minutes);
}

Result<TdSchedulerUserResult> perform_entrance(TdStore &store,
                                               Protocol::Td::ITdClient &client,
                                               const Model::Td::User &user,
                                               int index,
                                               Model::Td::UserState &state,
                                               const TdSchedulerRunOptions &options,
                                               std::vector<std::string> &logs) {
    const auto round_no = state.completed_rounds + 1;
    logs.push_back("用户 " + user.student_id + " 第 " + std::to_string(round_no) + "/" + std::to_string(user.rounds) + " 轮入口打卡");
    auto entrance = client.check(user, user.entrance_machine_id, options.timestamp_ms);
    if (!entrance) {
        mark_error(state, entrance.error().message);
        return user_result(index, user, false, false, true, entrance.error().message, state);
    }
    if (entrance->count) state.term_count = entrance->count;
    if (!entrance->success) {
        if (entrance->server_message.find("非法时间") != std::string::npos) {
            discard_current_round(state, user, "入口");
            logs.push_back("用户 " + user.student_id + " 入口非法时间，本轮作废");
            return user_result(index, user, true, false, true, "非法时间，本轮作废", state);
        }
        mark_error(state, "入口打卡失败: " + entrance->server_message);
        return user_result(index, user, false, false, true, state.last_error, state);
    }
    if (reached_limit(state.term_count)) {
        mark_completed(state, user, limit_message(user));
        return user_result(index, user, true, false, true, state.last_message, state);
    }

    auto photo = read_photo_bytes(store, user.entrance_image);
    if (!photo) {
        mark_error(state, photo.error().message);
        return user_result(index, user, false, false, true, photo.error().message, state);
    }
    auto uploaded = client.upload_photo(user.entrance_machine_id, *photo, options.timestamp_ms);
    if (!uploaded) {
        mark_error(state, uploaded.error().message);
        return user_result(index, user, false, false, true, uploaded.error().message, state);
    }

    const auto wait_minutes = wait_minutes_for_user(user, options);
    state.status = "waiting";
    state.next_action = "exit";
    state.next_run_at = add_minutes_to_iso(options.now_iso, wait_minutes);
    state.last_message = "入口完成，等待 " + std::to_string(wait_minutes) + " 分钟后出口";
    state.last_error.clear();
    logs.push_back("用户 " + user.student_id + " " + state.last_message);
    return user_result(index, user, true, false, true, state.last_message, state, wait_minutes * 60);
}

Result<TdSchedulerUserResult> perform_exit(TdStore &store,
                                           Protocol::Td::ITdClient &client,
                                           const Model::Td::User &user,
                                           int index,
                                           Model::Td::UserState &state,
                                           const TdSchedulerRunOptions &options,
                                           std::vector<std::string> &logs) {
    const auto round_no = state.completed_rounds + 1;
    logs.push_back("用户 " + user.student_id + " 第 " + std::to_string(round_no) + "/" + std::to_string(user.rounds) + " 轮出口打卡");
    auto exit = client.check(user, user.exit_machine_id, options.timestamp_ms);
    if (!exit) {
        mark_error(state, exit.error().message);
        return user_result(index, user, false, false, true, exit.error().message, state);
    }
    if (exit->count) state.term_count = exit->count;
    if (!exit->success) {
        if (exit->server_message.find("非法时间") != std::string::npos) {
            discard_current_round(state, user, "出口");
            logs.push_back("用户 " + user.student_id + " 出口非法时间，本轮作废");
            return user_result(index, user, true, false, true, "非法时间，本轮作废", state);
        }
        mark_error(state, "出口打卡失败: " + exit->server_message);
        return user_result(index, user, false, false, true, state.last_error, state);
    }

    auto photo = read_photo_bytes(store, user.exit_image);
    if (!photo) {
        mark_error(state, photo.error().message);
        return user_result(index, user, false, false, true, photo.error().message, state);
    }
    auto uploaded = client.upload_photo(user.exit_machine_id, *photo, options.timestamp_ms);
    if (!uploaded) {
        mark_error(state, uploaded.error().message);
        return user_result(index, user, false, false, true, uploaded.error().message, state);
    }

    ++state.completed_rounds;
    if (state.completed_rounds >= user.rounds || reached_limit(state.term_count)) {
        const auto message = reached_limit(state.term_count) ? limit_message(user)
                                                            : "今日已完成 " + std::to_string(state.completed_rounds) + "/" + std::to_string(user.rounds);
        mark_completed(state, user, message);
        logs.push_back("用户 " + user.student_id + " " + state.last_message);
        return user_result(index, user, true, false, true, state.last_message, state);
    }

    const auto wait_minutes = wait_minutes_for_user(user, options);
    state.status = "waiting";
    state.next_action = "entrance";
    state.next_run_at = add_minutes_to_iso(options.now_iso, wait_minutes);
    state.last_message = "出口完成，等待 " + std::to_string(wait_minutes) + " 分钟后下一轮入口";
    state.last_error.clear();
    logs.push_back("用户 " + user.student_id + " " + state.last_message);
    return user_result(index, user, true, false, true, state.last_message, state, wait_minutes * 60);
}

Result<TdSchedulerUserResult> perform_due_action(TdStore &store,
                                                 Protocol::Td::ITdClient &client,
                                                 const Model::Td::User &user,
                                                 int index,
                                                 Model::Td::UserState &state,
                                                 const TdSchedulerRunOptions &options,
                                                 std::vector<std::string> &logs) {
    if (state.completed_rounds >= user.rounds) {
        mark_completed(state, user, "今日已完成 " + std::to_string(user.rounds) + "/" + std::to_string(user.rounds));
        return user_result(index, user, true, true, false, "已完成，跳过", state);
    }
    if (state.next_action == "exit") return perform_exit(store, client, user, index, state, options, logs);
    return perform_entrance(store, client, user, index, state, options, logs);
}

void fill_summary_counts(TdSchedulerResult &result) {
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

Result<void> append_log_lines(const TdStore &store, const std::string &date, const std::vector<std::string> &lines) {
    if (lines.empty()) return Result<void>{};
    std::error_code error;
    std::filesystem::create_directories(store.paths().logs_dir, error);
    if (error) return make_error(ErrorCode::StorageError, "无法创建 TD 日志目录: " + store.paths().logs_dir.u8string() + ": " + error.message());
    const auto path = store.paths().logs_dir / (date + ".log");
    std::ofstream output(path, std::ios::app);
    if (!output) return make_error(ErrorCode::StorageError, "无法写入 TD 日志: " + path.u8string());
    for (const auto &line : lines) output << line << '\n';
    if (!output) return make_error(ErrorCode::StorageError, "写入 TD 日志失败: " + path.u8string());
    return Result<void>{};
}

} // namespace

TdSchedulerService::TdSchedulerService(TdStore &store, Protocol::Td::ITdClient &client) : m_store(store), m_client(client) {}

void TdSchedulerService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<TdScheduleWindow> TdSchedulerService::parse_window(const std::string &window) {
    const auto dash = window.find('-');
    if (dash == std::string::npos) return make_error(ErrorCode::InvalidArgument, "TD 时间窗口必须形如 HH:mm-HH:mm: " + window);
    auto start = hhmm_to_minutes(window.substr(0, dash));
    if (!start) return make_error(start.error().code, start.error().message);
    auto end = hhmm_to_minutes(window.substr(dash + 1));
    if (!end) return make_error(end.error().code, end.error().message);
    if (*start > *end) return make_error(ErrorCode::InvalidArgument, "TD 时间窗口开始时间不能晚于结束时间: " + window);
    return TdScheduleWindow{*start, *end};
}

Result<bool> TdSchedulerService::in_any_window(int minutes_since_midnight, const std::vector<std::string> &windows) {
    if (minutes_since_midnight < 0 || minutes_since_midnight >= 24 * 60) return make_error(ErrorCode::InvalidArgument, "minutes_since_midnight 超出范围");
    for (const auto &window : windows) {
        auto parsed = parse_window(window);
        if (!parsed) return make_error(parsed.error().code, parsed.error().message);
        if (minutes_since_midnight >= parsed->start_minutes && minutes_since_midnight <= parsed->end_minutes) return true;
    }
    return false;
}

Result<TdSchedulerResult> TdSchedulerService::run_once(const TdSchedulerRunOptions &input_options) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    auto initialized = m_store.initialize();
    if (!initialized) return make_error(initialized.error().code, initialized.error().message);

    auto config = m_store.load_config();
    if (!config) return make_error(config.error().code, config.error().message);
    auto users = m_store.load_users();
    if (!users) return make_error(users.error().code, users.error().message);
    auto previous_states = m_store.load_states();
    if (!previous_states) return make_error(previous_states.error().code, previous_states.error().message);

    TdSchedulerRunOptions options = input_options;
    if (options.timestamp_ms <= 0) options.timestamp_ms = default_timestamp_ms();
    if (options.date.empty()) options.date = default_date_string();
    if (options.now_iso.empty()) options.now_iso = default_iso_string();
    if (options.minutes_since_midnight < 0) options.minutes_since_midnight = default_minutes_since_midnight();

    auto in_window = in_any_window(options.minutes_since_midnight, config->windows);
    if (!in_window) return make_error(in_window.error().code, in_window.error().message);

    TdSchedulerResult result;
    result.date = options.date;
    result.now_iso = options.now_iso;
    result.in_window = *in_window;

    std::vector<Model::Td::UserState> active_states;
    active_states.reserve(users->size());
    for (const auto &user : *users) active_states.push_back(today_state(user, *previous_states, options.date));

    if (users->empty()) {
        auto saved = m_store.save_states({});
        if (!saved) return make_error(saved.error().code, saved.error().message);
        result.log_lines.push_back(options.now_iso + " 轮询心跳：当前没有用户，跳过");
        auto logged = append_log_lines(m_store, options.date, result.log_lines);
        if (!logged) return make_error(logged.error().code, logged.error().message);
        return result;
    }

    for (std::size_t i = 0; i < users->size(); ++i) {
        const auto &user = (*users)[i];
        auto &state = active_states[i];
        const int index = static_cast<int>(i + 1);

        if (!result.in_window) {
            if ((state.status == "pending" || state.status == "waiting") && !state.next_run_at.empty() && !due_after_now(state.next_run_at, options.now_iso)) {
                const auto action = state.next_action == "exit" ? std::string("出口") : std::string("入口");
                state.status = "pending";
                state.next_action = "entrance";
                state.next_run_at.clear();
                state.last_message = action + "到期时不在合法时间，等待下个窗口重新入口";
                state.last_error.clear();
                result.log_lines.push_back(options.now_iso + " 用户 " + user.student_id + " " + state.last_message);
            }
            result.users.push_back(user_result(index, user, state.status != "error", state.status == "completed", false, state.last_message.empty() ? state.status : state.last_message, state));
            continue;
        }

        if (state.status == "completed") {
            result.log_lines.push_back(options.now_iso + " 用户 " + user.student_id + " 今日已完成，跳过");
            result.users.push_back(user_result(index, user, true, true, false, "已完成，跳过", state));
            continue;
        }
        if (state.status == "error") {
            const auto message = state.last_error.empty() ? std::string("未知错误") : state.last_error;
            result.log_lines.push_back(options.now_iso + " 用户 " + user.student_id + " 状态为 error，等待清除错误后重试：" + message);
            result.users.push_back(user_result(index, user, false, false, false, message, state));
            continue;
        }
        if (reached_limit(known_term_count(user, state))) {
            state.term_count = known_term_count(user, state);
            mark_completed(state, user, limit_message(user));
            result.log_lines.push_back(options.now_iso + " " + state.last_message);
            result.users.push_back(user_result(index, user, true, true, false, state.last_message, state));
            continue;
        }
        if (!state.next_run_at.empty() && due_after_now(state.next_run_at, options.now_iso)) {
            const auto remaining = remaining_seconds(options.now_iso, state.next_run_at);
            const auto message = "等待 " + std::to_string(remaining) + " 秒";
            result.log_lines.push_back(options.now_iso + " 用户 " + user.student_id + " " + message);
            result.users.push_back(user_result(index, user, true, false, false, message, state, remaining));
            continue;
        }

        auto user_tick = perform_due_action(m_store, m_client, user, index, state, options, result.log_lines);
        if (!user_tick) {
            mark_error(state, user_tick.error().message);
            result.users.push_back(user_result(index, user, false, false, true, user_tick.error().message, state));
            continue;
        }
        result.users.push_back(*user_tick);
    }

    auto saved = m_store.save_states(active_states);
    if (!saved) return make_error(saved.error().code, saved.error().message);
    auto logged = append_log_lines(m_store, options.date, result.log_lines);
    if (!logged) return make_error(logged.error().code, logged.error().message);
    fill_summary_counts(result);
    return result;
}

Result<int> TdSchedulerService::clear_today_errors(std::string date, std::string now_iso) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);

    auto initialized = m_store.initialize();
    if (!initialized) return make_error(initialized.error().code, initialized.error().message);
    if (date.empty()) date = default_date_string();
    if (now_iso.empty()) now_iso = default_iso_string();

    auto states = m_store.load_states();
    if (!states) return make_error(states.error().code, states.error().message);
    int changed = 0;
    for (auto &state : *states) {
        if (state.date == date && state.status == "error") {
            state.status = "pending";
            state.next_action = "entrance";
            state.next_run_at.clear();
            state.last_error.clear();
            state.last_message = "错误已清除，等待重新入口打卡";
            ++changed;
        }
    }
    if (changed > 0) {
        auto saved = m_store.save_states(*states);
        if (!saved) return make_error(saved.error().code, saved.error().message);
        auto logged = append_log_lines(m_store, date, {now_iso + " 清除 TD 今日错误状态 " + std::to_string(changed) + " 条"});
        if (!logged) return make_error(logged.error().code, logged.error().message);
    }
    return changed;
}

std::vector<Model::FeatureRecord> td_scheduler_records(const TdSchedulerResult &result) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(result.users.size() + 1);
    Model::FeatureRecord summary;
    summary.id = "td";
    summary.title = "TD scheduler";
    summary.status = result.total == 0 ? "empty" : (result.failure_count > 0 ? "partial" : "ok");
    summary.fields["date"] = result.date;
    summary.fields["now"] = result.now_iso;
    summary.fields["inWindow"] = result.in_window ? "true" : "false";
    summary.fields["total"] = int_text(result.total);
    summary.fields["success"] = int_text(result.success_count);
    summary.fields["failure"] = int_text(result.failure_count);
    summary.fields["skipped"] = int_text(result.skipped_count);
    records.push_back(std::move(summary));

    for (const auto &user : result.users) {
        Model::FeatureRecord record;
        record.id = user.student_id;
        record.title = user.student_id;
        record.status = user.status;
        record.fields["index"] = int_text(user.index);
        record.fields["success"] = user.success ? "true" : "false";
        record.fields["skipped"] = user.skipped ? "true" : "false";
        record.fields["remoteRequest"] = user.remote_request ? "true" : "false";
        record.fields["message"] = user.message;
        record.fields["completedRounds"] = int_text(user.completed_rounds);
        record.fields["termCount"] = count_text(user.term_count);
        record.fields["nextAction"] = user.state.next_action;
        record.fields["nextRunAt"] = user.state.next_run_at;
        record.fields["remainingSeconds"] = int_text(user.remaining_seconds);
        record.fields["lastError"] = user.state.last_error;
        record.fields["lastMessage"] = user.state.last_message;
        records.push_back(std::move(record));
    }
    return records;
}

std::vector<Model::FeatureRecord> td_state_records(const std::vector<Model::Td::UserState> &states) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(states.size());
    for (const auto &state : states) {
        Model::FeatureRecord record;
        record.id = state.student_id;
        record.title = state.date.empty() ? state.student_id : state.date;
        record.status = state.status;
        record.fields["studentId"] = state.student_id;
        record.fields["date"] = state.date;
        record.fields["nextAction"] = state.next_action;
        record.fields["completedRounds"] = int_text(state.completed_rounds);
        record.fields["termCount"] = count_text(state.term_count);
        record.fields["nextRunAt"] = state.next_run_at;
        record.fields["lastError"] = state.last_error;
        record.fields["lastMessage"] = state.last_message;
        records.push_back(std::move(record));
    }
    return records;
}

} // namespace UBAANext
