#include <UBAANext/Model/Td.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>

namespace UBAANext {
namespace Model {
namespace Td {
namespace {

std::string trim(std::string value) {
    auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string ascii_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string uppercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

bool contains(std::string_view text, std::string_view needle) {
    return text.find(needle) != std::string_view::npos;
}

std::string string_value(const nlohmann::json &json, std::string_view key, std::string fallback = {}) {
    auto it = json.find(std::string(key));
    if (it == json.end() || it->is_null()) return fallback;
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number_integer()) return std::to_string(it->get<long long>());
    if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
    if (it->is_number_float()) return std::to_string(it->get<double>());
    if (it->is_boolean()) return *it ? "true" : "false";
    return fallback;
}

Result<int> int_value(const nlohmann::json &json, std::string_view key, std::optional<int> fallback = std::nullopt) {
    auto it = json.find(std::string(key));
    if (it == json.end() || it->is_null()) {
        if (fallback) return *fallback;
        return make_error(ErrorCode::InvalidArgument, std::string(key) + " 不能为空");
    }
    if (it->is_number_integer()) return it->get<int>();
    if (it->is_number_unsigned()) return static_cast<int>(it->get<unsigned int>());
    if (it->is_string()) {
        const auto text = trim(it->get<std::string>());
        int parsed = 0;
        const auto *begin = text.data();
        const auto *end = text.data() + text.size();
        auto result = std::from_chars(begin, end, parsed);
        if (result.ec == std::errc{} && result.ptr == end) return parsed;
    }
    return make_error(ErrorCode::InvalidArgument, std::string(key) + " 必须是整数");
}

std::string image_name(std::string value) {
    value = trim(std::move(value));
    const auto slash = value.find_last_of("/\\");
    if (slash != std::string::npos) value = value.substr(slash + 1);
    return value;
}

Result<std::string> derive_card_id(const std::string &student_id) {
    unsigned long long number = 0;
    const auto *begin = student_id.data();
    const auto *end = student_id.data() + student_id.size();
    auto result = std::from_chars(begin, end, number);
    if (student_id.empty() || result.ec != std::errc{} || result.ptr != end) {
        return make_error(ErrorCode::InvalidArgument, "student_id 必须是可转换为 card_id 的十进制数字");
    }

    std::ostringstream output;
    output << std::uppercase << std::hex << number;
    return output.str();
}

bool is_campus_machine(const Machine &machine, const std::string &campus) {
    if (campus == "shahe") return contains(machine.location, "沙河");
    return contains(machine.location, "学院路") || contains(machine.location, "本部");
}

bool is_direction_machine(const Machine &machine, bool entrance) {
    if (entrance) return machine.door_type == "1" || contains(machine.location, "入口");
    return machine.door_type == "2" || contains(machine.location, "出口");
}

nlohmann::json machine_to_json(const Machine &machine) {
    return nlohmann::json{{"id", machine.id},
                          {"machinesn", machine.serial_number},
                          {"location", machine.location},
                          {"doortype", machine.door_type}};
}

Result<Machine> machine_from_json(const nlohmann::json &json) {
    if (!json.is_object()) return make_error(ErrorCode::InvalidArgument, "machine 必须是对象");
    auto id = int_value(json, "id");
    if (!id) return make_error(id.error().code, id.error().message);
    Machine machine;
    machine.id = id.value();
    machine.serial_number = string_value(json, "machinesn");
    machine.location = string_value(json, "location");
    machine.door_type = string_value(json, "doortype");
    if (machine.id <= 0) return make_error(ErrorCode::InvalidArgument, "machine.id 必须大于 0");
    if (machine.serial_number.empty()) return make_error(ErrorCode::InvalidArgument, "machine.machinesn 不能为空");
    if (machine.location.empty()) return make_error(ErrorCode::InvalidArgument, "machine.location 不能为空");
    return machine;
}

} // namespace

Config default_config() {
    Config config;
    config.machines = {
        {2, "20211025001", "北航本部TD入口1", "1"},
        {3, "20220301004", "北航本部TD入口3", "1"},
        {4, "20230417001", "北航本部TD入口2", "1"},
        {5, "20210421003", "北航本部TD出口2", "2"},
        {6, "20210420002", "北航本部TD出口1", "2"},
        {7, "20220301003", "北航本部TD出口3", "2"},
        {8, "20210511001", "北航沙河TD入口1", "1"},
        {9, "20210511002", "北航沙河TD入口2", "1"},
        {10, "20210511003", "北航沙河TD入口3", "1"},
        {11, "20220218001", "北航沙河TD出口1", "2"},
        {12, "20220218002", "北航沙河TD出口2", "2"},
        {13, "20220218003", "北航沙河TD出口3", "2"},
    };
    return config;
}

Result<std::string> normalize_campus(std::string campus) {
    campus = ascii_lower(trim(std::move(campus)));
    if (campus == "沙河" || campus == "shahe" || campus == "sh") return std::string("shahe");
    if (campus == "学院路" || campus == "學院路" || campus == "本部" || campus == "xueyuanlu" || campus == "xyl") return std::string("xueyuanlu");
    return make_error(ErrorCode::InvalidArgument, "quick 只支持 沙河 或 学院路");
}

std::vector<Machine> machine_pool(const std::vector<Machine> &machines, const std::string &normalized_campus, bool entrance) {
    std::vector<Machine> pool;
    for (const auto &machine : machines) {
        if (is_campus_machine(machine, normalized_campus) && is_direction_machine(machine, entrance)) pool.push_back(machine);
    }
    std::sort(pool.begin(), pool.end(), [](const Machine &left, const Machine &right) { return left.id < right.id; });
    return pool;
}

Result<User> make_user(std::string student_id,
                       std::string card_id,
                       int entrance_machine_id,
                       int exit_machine_id,
                       std::string entrance_image,
                       std::string exit_image,
                       int rounds,
                       int wait_time_min_minutes,
                       int wait_time_max_minutes,
                       std::optional<int> cached_term_count) {
    User user;
    user.student_id = trim(std::move(student_id));
    if (user.student_id.empty()) return make_error(ErrorCode::InvalidArgument, "student_id 不能为空");

    card_id = trim(std::move(card_id));
    if (card_id.empty()) {
        auto derived = derive_card_id(user.student_id);
        if (!derived) return make_error(derived.error().code, derived.error().message);
        user.card_id = derived.value();
    } else {
        user.card_id = uppercase_ascii(std::move(card_id));
    }

    user.entrance_machine_id = entrance_machine_id;
    user.exit_machine_id = exit_machine_id;
    user.entrance_image = image_name(std::move(entrance_image));
    user.exit_image = image_name(std::move(exit_image));
    user.rounds = rounds;
    user.wait_time_min_minutes = wait_time_min_minutes;
    user.wait_time_max_minutes = wait_time_max_minutes;
    user.cached_term_count = cached_term_count;

    if (user.entrance_machine_id <= 0) return make_error(ErrorCode::InvalidArgument, "entrance_machine_id 必须大于 0");
    if (user.exit_machine_id <= 0) return make_error(ErrorCode::InvalidArgument, "exit_machine_id 必须大于 0");
    if (user.entrance_image.empty() || user.exit_image.empty()) return make_error(ErrorCode::InvalidArgument, "入口和出口图片不能为空");
    if (user.rounds <= 0) return make_error(ErrorCode::InvalidArgument, "rounds 必须大于 0");
    if (user.wait_time_min_minutes < 0 || user.wait_time_max_minutes < 0) return make_error(ErrorCode::InvalidArgument, "wait_time_min 和 wait_time_max 不能为负数");
    if (user.wait_time_min_minutes > user.wait_time_max_minutes) return make_error(ErrorCode::InvalidArgument, "wait_time_min 必须小于等于 wait_time_max");
    if (user.cached_term_count && *user.cached_term_count < 0) return make_error(ErrorCode::InvalidArgument, "cached_term_count 不能为负数");
    return user;
}

Result<User> build_quick_user(const Config &config,
                              const std::vector<std::string> &images,
                              std::string student_id,
                              std::string campus,
                              std::size_t entrance_index,
                              std::size_t exit_index,
                              std::size_t entrance_image_index,
                              std::size_t exit_image_index,
                              std::string card_id) {
    auto normalized = normalize_campus(std::move(campus));
    if (!normalized) return make_error(normalized.error().code, normalized.error().message);
    auto entrances = machine_pool(config.machines, normalized.value(), true);
    auto exits = machine_pool(config.machines, normalized.value(), false);
    if (entrances.empty()) return make_error(ErrorCode::InvalidArgument, "未找到该校区的入口机");
    if (exits.empty()) return make_error(ErrorCode::InvalidArgument, "未找到该校区的出口机");
    if (images.empty()) return make_error(ErrorCode::InvalidArgument, "没有可用图片，请先添加 TD 图片");

    return make_user(std::move(student_id),
                     std::move(card_id),
                     entrances[entrance_index % entrances.size()].id,
                     exits[exit_index % exits.size()].id,
                     images[entrance_image_index % images.size()],
                     images[exit_image_index % images.size()]);
}

Result<User> user_from_json(const nlohmann::json &json) {
    if (!json.is_object()) return make_error(ErrorCode::InvalidArgument, "TD 用户必须是对象");
    auto entrance_machine_id = int_value(json, "entrance_machine_id");
    if (!entrance_machine_id) return make_error(entrance_machine_id.error().code, entrance_machine_id.error().message);
    auto exit_machine_id = int_value(json, "exit_machine_id");
    if (!exit_machine_id) return make_error(exit_machine_id.error().code, exit_machine_id.error().message);
    auto rounds = int_value(json, "rounds", default_rounds);
    if (!rounds) return make_error(rounds.error().code, rounds.error().message);
    auto wait_min = int_value(json, "wait_time_min", default_wait_time_min_minutes);
    if (!wait_min) return make_error(wait_min.error().code, wait_min.error().message);
    auto wait_max = int_value(json, "wait_time_max", default_wait_time_max_minutes);
    if (!wait_max) return make_error(wait_max.error().code, wait_max.error().message);

    std::optional<int> cached_term_count;
    if (json.contains("cached_term_count") && !json["cached_term_count"].is_null()) {
        auto count = int_value(json, "cached_term_count");
        if (!count) return make_error(count.error().code, count.error().message);
        cached_term_count = count.value();
    }

    auto entrance_image = string_value(json, "entrance_image");
    if (entrance_image.empty()) entrance_image = string_value(json, "entrance_photo_path");
    auto exit_image = string_value(json, "exit_image");
    if (exit_image.empty()) exit_image = string_value(json, "exit_photo_path");

    return make_user(string_value(json, "student_id"),
                     string_value(json, "card_id"),
                     entrance_machine_id.value(),
                     exit_machine_id.value(),
                     entrance_image,
                     exit_image,
                     rounds.value(),
                     wait_min.value(),
                     wait_max.value(),
                     cached_term_count);
}

nlohmann::json user_to_json(const User &user) {
    auto json = nlohmann::json{{"student_id", user.student_id},
                               {"card_id", user.card_id},
                               {"entrance_machine_id", user.entrance_machine_id},
                               {"exit_machine_id", user.exit_machine_id},
                               {"entrance_image", user.entrance_image},
                               {"exit_image", user.exit_image},
                               {"rounds", user.rounds},
                               {"wait_time_min", user.wait_time_min_minutes},
                               {"wait_time_max", user.wait_time_max_minutes}};
    if (user.cached_term_count) json["cached_term_count"] = *user.cached_term_count;
    return json;
}

Result<Config> config_from_json(const nlohmann::json &json) {
    if (!json.is_object()) return make_error(ErrorCode::InvalidArgument, "TD 配置必须是对象");
    Config config = default_config();
    auto type = int_value(json, "type", default_type);
    if (!type) return make_error(type.error().code, type.error().message);
    config.type = type.value();
    config.school_number = string_value(json, "schoolno", config.school_number);
    config.event_number = string_value(json, "eventno", config.event_number);
    auto poll = int_value(json, "poll_seconds", default_poll_seconds);
    if (!poll) return make_error(poll.error().code, poll.error().message);
    config.poll_seconds = poll.value();

    if (json.contains("server")) {
        const auto &server = json["server"];
        if (!server.is_object()) return make_error(ErrorCode::InvalidArgument, "server 必须是对象");
        config.server.ip = string_value(server, "ip", config.server.ip);
        auto port = int_value(server, "port", config.server.port);
        if (!port) return make_error(port.error().code, port.error().message);
        config.server.port = port.value();
        auto timeout = int_value(server, "timeout", config.server.timeout_seconds);
        if (!timeout) return make_error(timeout.error().code, timeout.error().message);
        config.server.timeout_seconds = timeout.value();
    }

    if (json.contains("windows")) {
        if (!json["windows"].is_array()) return make_error(ErrorCode::InvalidArgument, "windows 必须是数组");
        config.windows.clear();
        for (const auto &window : json["windows"]) {
            if (!window.is_string()) return make_error(ErrorCode::InvalidArgument, "windows 项必须是字符串");
            config.windows.push_back(window.get<std::string>());
        }
    }

    if (json.contains("machine")) {
        if (!json["machine"].is_array()) return make_error(ErrorCode::InvalidArgument, "machine 必须是数组");
        config.machines.clear();
        for (const auto &entry : json["machine"]) {
            auto machine = machine_from_json(entry);
            if (!machine) return make_error(machine.error().code, machine.error().message);
            config.machines.push_back(machine.value());
        }
    }

    if (config.server.ip.empty()) return make_error(ErrorCode::InvalidArgument, "server.ip 不能为空");
    if (config.server.port <= 0) return make_error(ErrorCode::InvalidArgument, "server.port 必须大于 0");
    if (config.server.timeout_seconds <= 0) return make_error(ErrorCode::InvalidArgument, "server.timeout 必须大于 0");
    if (config.poll_seconds <= 0) return make_error(ErrorCode::InvalidArgument, "poll_seconds 必须大于 0");
    if (config.machines.empty()) return make_error(ErrorCode::InvalidArgument, "machine 不能为空");
    return config;
}

nlohmann::json config_to_json(const Config &config) {
    nlohmann::json machines = nlohmann::json::array();
    for (const auto &machine : config.machines) machines.push_back(machine_to_json(machine));
    return nlohmann::json{{"type", config.type},
                          {"schoolno", config.school_number},
                          {"eventno", config.event_number},
                          {"server", {{"ip", config.server.ip}, {"port", config.server.port}, {"timeout", config.server.timeout_seconds}}},
                          {"windows", config.windows},
                          {"poll_seconds", config.poll_seconds},
                          {"machine", machines}};
}

Result<UserState> state_from_json(const nlohmann::json &json) {
    if (!json.is_object()) return make_error(ErrorCode::InvalidArgument, "TD 状态必须是对象");
    UserState state;
    state.student_id = string_value(json, "student_id");
    state.date = string_value(json, "date");
    state.status = string_value(json, "status", state.status);
    state.next_action = string_value(json, "next_action", state.next_action);
    auto rounds = int_value(json, "completed_rounds", 0);
    if (!rounds) return make_error(rounds.error().code, rounds.error().message);
    state.completed_rounds = rounds.value();
    if (json.contains("term_count") && !json["term_count"].is_null()) {
        auto count = int_value(json, "term_count");
        if (!count) return make_error(count.error().code, count.error().message);
        state.term_count = count.value();
    }
    state.next_run_at = string_value(json, "next_run_at");
    state.last_error = string_value(json, "last_error");
    state.last_message = string_value(json, "last_message");
    if (state.student_id.empty()) return make_error(ErrorCode::InvalidArgument, "state.student_id 不能为空");
    if (state.completed_rounds < 0) return make_error(ErrorCode::InvalidArgument, "completed_rounds 不能为负数");
    if (state.term_count && *state.term_count < 0) return make_error(ErrorCode::InvalidArgument, "term_count 不能为负数");
    return state;
}

nlohmann::json state_to_json(const UserState &state) {
    auto json = nlohmann::json{{"student_id", state.student_id},
                               {"date", state.date},
                               {"status", state.status},
                               {"next_action", state.next_action},
                               {"completed_rounds", state.completed_rounds},
                               {"next_run_at", state.next_run_at},
                               {"last_error", state.last_error},
                               {"last_message", state.last_message}};
    if (state.term_count) json["term_count"] = *state.term_count;
    return json;
}

} // namespace Td
} // namespace Model
} // namespace UBAANext
