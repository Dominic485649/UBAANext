#pragma once

#include <UBAANext/Base/Result.hpp>

#include <nlohmann/json_fwd.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace UBAANext {
namespace Model {
namespace Td {

constexpr int completion_limit = 32;
constexpr int default_type = 1;
constexpr int default_port = 8888;
constexpr int default_timeout_seconds = 10;
constexpr int default_poll_seconds = 60;
constexpr int default_rounds = 3;
constexpr int default_wait_time_min_minutes = 180;
constexpr int default_wait_time_max_minutes = 240;

struct ServerConfig {
    std::string ip = "10.212.28.38";
    int port = default_port;
    int timeout_seconds = default_timeout_seconds;
};

struct Machine {
    int id = 0;
    std::string serial_number;
    std::string location;
    std::string door_type;
};

struct Config {
    int type = default_type;
    std::string school_number = "10006";
    std::string event_number = "802";
    ServerConfig server;
    std::vector<std::string> windows = {"07:30-10:00", "11:30-14:00", "15:30-20:00"};
    int poll_seconds = default_poll_seconds;
    std::vector<Machine> machines;
};

struct User {
    std::string student_id;
    std::string card_id;
    int entrance_machine_id = 0;
    int exit_machine_id = 0;
    std::string entrance_image;
    std::string exit_image;
    int rounds = default_rounds;
    int wait_time_min_minutes = default_wait_time_min_minutes;
    int wait_time_max_minutes = default_wait_time_max_minutes;
    std::optional<int> cached_term_count;
};

struct UserState {
    std::string student_id;
    std::string date;
    std::string status = "pending";
    std::string next_action = "entrance";
    int completed_rounds = 0;
    std::optional<int> term_count;
    std::string next_run_at;
    std::string last_error;
    std::string last_message;
};

struct QuickSelection {
    User user;
    std::string campus;
};

[[nodiscard]] Config default_config();
[[nodiscard]] Result<std::string> normalize_campus(std::string campus);
[[nodiscard]] std::vector<Machine> machine_pool(const std::vector<Machine> &machines,
                                                const std::string &normalized_campus,
                                                bool entrance);
[[nodiscard]] Result<User> make_user(std::string student_id,
                                     std::string card_id,
                                     int entrance_machine_id,
                                     int exit_machine_id,
                                     std::string entrance_image,
                                     std::string exit_image,
                                     int rounds = default_rounds,
                                     int wait_time_min_minutes = default_wait_time_min_minutes,
                                     int wait_time_max_minutes = default_wait_time_max_minutes,
                                     std::optional<int> cached_term_count = std::nullopt);
[[nodiscard]] Result<User> build_quick_user(const Config &config,
                                            const std::vector<std::string> &images,
                                            std::string student_id,
                                            std::string campus,
                                            std::size_t entrance_index = 0,
                                            std::size_t exit_index = 0,
                                            std::size_t entrance_image_index = 0,
                                            std::size_t exit_image_index = 0,
                                            std::string card_id = "");

[[nodiscard]] Result<User> user_from_json(const nlohmann::json &json);
[[nodiscard]] nlohmann::json user_to_json(const User &user);
[[nodiscard]] Result<Config> config_from_json(const nlohmann::json &json);
[[nodiscard]] nlohmann::json config_to_json(const Config &config);
[[nodiscard]] Result<UserState> state_from_json(const nlohmann::json &json);
[[nodiscard]] nlohmann::json state_to_json(const UserState &state);

} // namespace Td
} // namespace Model
} // namespace UBAANext
