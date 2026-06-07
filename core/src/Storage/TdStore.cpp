#include <UBAANext/Storage/TdStore.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace UBAANext {
namespace {

Unexpected filesystem_error(std::string action, const std::filesystem::path &path, const std::error_code &error) {
    return make_error(ErrorCode::StorageError, action + ": " + path.u8string() + ": " + error.message());
}

Result<void> ensure_directory(const std::filesystem::path &path) {
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error) return filesystem_error("无法创建目录", path, error);
    if (!std::filesystem::is_directory(path, error)) return make_error(ErrorCode::StorageError, "路径不是目录: " + path.u8string());
    return Result<void>{};
}

Result<bool> path_exists(const std::filesystem::path &path) {
    std::error_code error;
    const bool value = std::filesystem::exists(path, error);
    if (error) return filesystem_error("无法访问路径", path, error);
    return value;
}

Result<bool> path_is_regular_file(const std::filesystem::path &path) {
    std::error_code error;
    const bool value = std::filesystem::is_regular_file(path, error);
    if (error) return filesystem_error("无法检查文件", path, error);
    return value;
}

Result<nlohmann::json> read_json_file(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input) return make_error(ErrorCode::StorageError, "无法读取 JSON 文件: " + path.u8string());
    try {
        nlohmann::json json;
        input >> json;
        return json;
    } catch (const nlohmann::json::exception &error) {
        return make_error(ErrorCode::ParseError, "JSON 文件解析失败: " + path.u8string() + ": " + error.what());
    }
}

Result<void> write_json_file(const std::filesystem::path &path, const nlohmann::json &json) {
    auto parent = ensure_directory(path.parent_path());
    if (!parent) return make_error(parent.error().code, parent.error().message);

    std::ofstream output(path, std::ios::trunc);
    if (!output) return make_error(ErrorCode::StorageError, "无法写入 JSON 文件: " + path.u8string());
    output << json.dump(2) << '\n';
    if (!output) return make_error(ErrorCode::StorageError, "写入 JSON 文件失败: " + path.u8string());
    return Result<void>{};
}

Result<void> write_json_file_if_missing(const std::filesystem::path &path, const nlohmann::json &json) {
    auto present = path_exists(path);
    if (!present) return make_error(present.error().code, present.error().message);
    if (present.value()) return Result<void>{};
    return write_json_file(path, json);
}

nlohmann::json default_settings_json() {
    const auto config = Model::Td::default_config();
    return nlohmann::json{{"schedule", {{"poll_seconds", config.poll_seconds}, {"windows", config.windows}}}};
}

nlohmann::json default_states_json() {
    return nlohmann::json{{"states", nlohmann::json::object()}};
}

nlohmann::json default_users_json() {
    return nlohmann::json{{"users", nlohmann::json::object()}};
}

Result<nlohmann::json> load_object_file_or_default(const std::filesystem::path &path, nlohmann::json fallback) {
    auto present = path_exists(path);
    if (!present) return make_error(present.error().code, present.error().message);
    if (!present.value()) return fallback;
    auto json = read_json_file(path);
    if (!json) return make_error(json.error().code, json.error().message);
    if (!json->is_object()) return make_error(ErrorCode::ParseError, "JSON 文件顶层必须是对象: " + path.u8string());
    return json.value();
}

Result<std::string> safe_file_name(std::string name, std::string label) {
    if (name.empty()) return make_error(ErrorCode::InvalidArgument, label + "不能为空");
    const std::filesystem::path path(name);
    if (path.filename().u8string() != name || path.has_parent_path()) {
        return make_error(ErrorCode::InvalidArgument, label + "不能包含目录");
    }
    if (name == "." || name == "..") return make_error(ErrorCode::InvalidArgument, label + "非法");
    return name;
}

std::string student_id_key(const std::string &student_id) {
    return student_id;
}

} // namespace

TdStore::TdStore(std::filesystem::path root) {
    m_paths.root = std::move(root);
    m_paths.images_dir = m_paths.root / "images";
    m_paths.logs_dir = m_paths.root / "logs";
    m_paths.config_path = m_paths.root / "config.json";
    m_paths.users_path = m_paths.root / "users.json";
    m_paths.settings_path = m_paths.root / "settings.json";
    m_paths.state_path = m_paths.root / "state.json";
}

Result<TdStore> TdStore::from_app_data_dir(const IAppDataPathProvider &provider) {
    auto app_data = provider.app_data_dir();
    if (!app_data) return make_error(app_data.error().code, app_data.error().message);
    return TdStore(app_data.value() / "td");
}

const TdStorePaths &TdStore::paths() const noexcept {
    return m_paths;
}

Result<void> TdStore::initialize() const {
    auto root = ensure_directory(m_paths.root);
    if (!root) return make_error(root.error().code, root.error().message);
    auto images = ensure_directory(m_paths.images_dir);
    if (!images) return make_error(images.error().code, images.error().message);
    auto logs = ensure_directory(m_paths.logs_dir);
    if (!logs) return make_error(logs.error().code, logs.error().message);

    auto config = write_json_file_if_missing(m_paths.config_path, Model::Td::config_to_json(Model::Td::default_config()));
    if (!config) return make_error(config.error().code, config.error().message);
    auto users = write_json_file_if_missing(m_paths.users_path, default_users_json());
    if (!users) return make_error(users.error().code, users.error().message);
    auto settings = write_json_file_if_missing(m_paths.settings_path, default_settings_json());
    if (!settings) return make_error(settings.error().code, settings.error().message);
    auto state = write_json_file_if_missing(m_paths.state_path, default_states_json());
    if (!state) return make_error(state.error().code, state.error().message);
    return Result<void>{};
}

Result<Model::Td::Config> TdStore::load_config() const {
    auto json = read_json_file(m_paths.config_path);
    if (!json) return make_error(json.error().code, json.error().message);
    return Model::Td::config_from_json(json.value());
}

Result<void> TdStore::save_config(const Model::Td::Config &config) const {
    return write_json_file(m_paths.config_path, Model::Td::config_to_json(config));
}

Result<std::vector<Model::Td::User>> TdStore::load_users() const {
    auto json = load_object_file_or_default(m_paths.users_path, default_users_json());
    if (!json) return make_error(json.error().code, json.error().message);
    const auto users_it = json->find("users");
    if (users_it == json->end() || users_it->is_null()) return std::vector<Model::Td::User>{};
    if (!users_it->is_object()) return make_error(ErrorCode::ParseError, "users.json 中 users 必须是对象");

    std::vector<Model::Td::User> users;
    for (const auto &entry : users_it->items()) {
        auto user = Model::Td::user_from_json(entry.value());
        if (!user) return make_error(user.error().code, user.error().message);
        users.push_back(user.value());
    }
    std::sort(users.begin(), users.end(), [](const Model::Td::User &left, const Model::Td::User &right) {
        return left.student_id < right.student_id;
    });
    return users;
}

Result<std::optional<Model::Td::User>> TdStore::load_user(const std::string &student_id) const {
    auto users = load_users();
    if (!users) return make_error(users.error().code, users.error().message);
    const auto key = student_id_key(student_id);
    for (const auto &user : users.value()) {
        if (user.student_id == key) return std::optional<Model::Td::User>(user);
    }
    return std::optional<Model::Td::User>{};
}

Result<void> TdStore::save_user(const Model::Td::User &user, bool allow_update) const {
    auto validation = validate_user_references(user);
    if (!validation) return make_error(validation.error().code, validation.error().message);

    auto json = load_object_file_or_default(m_paths.users_path, default_users_json());
    if (!json) return make_error(json.error().code, json.error().message);
    auto &users = (*json)["users"];
    if (users.is_null()) users = nlohmann::json::object();
    if (!users.is_object()) return make_error(ErrorCode::ParseError, "users.json 中 users 必须是对象");

    const auto key = student_id_key(user.student_id);
    if (!allow_update && users.contains(key)) return make_error(ErrorCode::InvalidArgument, "TD 用户已存在: " + key);
    users[key] = Model::Td::user_to_json(user);
    return write_json_file(m_paths.users_path, json.value());
}

Result<bool> TdStore::delete_user(const std::string &student_id) const {
    auto json = load_object_file_or_default(m_paths.users_path, default_users_json());
    if (!json) return make_error(json.error().code, json.error().message);
    auto &users = (*json)["users"];
    if (users.is_null()) users = nlohmann::json::object();
    if (!users.is_object()) return make_error(ErrorCode::ParseError, "users.json 中 users 必须是对象");

    const auto removed = users.erase(student_id_key(student_id)) > 0;
    auto saved = write_json_file(m_paths.users_path, json.value());
    if (!saved) return make_error(saved.error().code, saved.error().message);
    return removed;
}

Result<std::vector<Model::Td::UserState>> TdStore::load_states() const {
    auto json = load_object_file_or_default(m_paths.state_path, default_states_json());
    if (!json) return make_error(json.error().code, json.error().message);
    const auto states_it = json->find("states");
    if (states_it == json->end() || states_it->is_null()) return std::vector<Model::Td::UserState>{};
    if (!states_it->is_object()) return make_error(ErrorCode::ParseError, "state.json 中 states 必须是对象");

    std::vector<Model::Td::UserState> states;
    for (const auto &entry : states_it->items()) {
        auto state = Model::Td::state_from_json(entry.value());
        if (!state) return make_error(state.error().code, state.error().message);
        states.push_back(state.value());
    }
    std::sort(states.begin(), states.end(), [](const Model::Td::UserState &left, const Model::Td::UserState &right) {
        return left.student_id < right.student_id;
    });
    return states;
}

Result<std::optional<Model::Td::UserState>> TdStore::load_state(const std::string &student_id) const {
    auto states = load_states();
    if (!states) return make_error(states.error().code, states.error().message);
    const auto key = student_id_key(student_id);
    for (const auto &state : states.value()) {
        if (state.student_id == key) return std::optional<Model::Td::UserState>(state);
    }
    return std::optional<Model::Td::UserState>{};
}

Result<void> TdStore::save_states(const std::vector<Model::Td::UserState> &states) const {
    nlohmann::json state_map = nlohmann::json::object();
    for (const auto &state : states) {
        auto serialized = Model::Td::state_to_json(state);
        auto parsed = Model::Td::state_from_json(serialized);
        if (!parsed) return make_error(parsed.error().code, parsed.error().message);
        state_map[state.student_id] = std::move(serialized);
    }
    return write_json_file(m_paths.state_path, nlohmann::json{{"states", state_map}});
}

Result<void> TdStore::save_state(const Model::Td::UserState &state) const {
    auto parsed = Model::Td::state_from_json(Model::Td::state_to_json(state));
    if (!parsed) return make_error(parsed.error().code, parsed.error().message);

    auto json = load_object_file_or_default(m_paths.state_path, default_states_json());
    if (!json) return make_error(json.error().code, json.error().message);
    auto &states = (*json)["states"];
    if (states.is_null()) states = nlohmann::json::object();
    if (!states.is_object()) return make_error(ErrorCode::ParseError, "state.json 中 states 必须是对象");

    states[state.student_id] = Model::Td::state_to_json(state);
    return write_json_file(m_paths.state_path, json.value());
}

Result<std::string> TdStore::add_image(const std::filesystem::path &source, std::string name, bool overwrite) const {
    auto source_present = path_exists(source);
    if (!source_present) return make_error(source_present.error().code, source_present.error().message);
    if (!source_present.value()) return make_error(ErrorCode::InvalidArgument, "图片源文件不存在: " + source.u8string());
    auto source_file = path_is_regular_file(source);
    if (!source_file) return make_error(source_file.error().code, source_file.error().message);
    if (!source_file.value()) return make_error(ErrorCode::InvalidArgument, "图片源路径不是普通文件: " + source.u8string());

    if (name.empty()) name = source.filename().u8string();
    auto safe_name = safe_file_name(std::move(name), "图片名称");
    if (!safe_name) return make_error(safe_name.error().code, safe_name.error().message);

    auto images = ensure_directory(m_paths.images_dir);
    if (!images) return make_error(images.error().code, images.error().message);
    const auto target = m_paths.images_dir / safe_name.value();
    auto target_present = path_exists(target);
    if (!target_present) return make_error(target_present.error().code, target_present.error().message);
    if (target_present.value() && !overwrite) return make_error(ErrorCode::InvalidArgument, "图片已存在: " + safe_name.value());

    std::error_code error;
    std::filesystem::copy_file(source,
                               target,
                               overwrite ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none,
                               error);
    if (error) return filesystem_error("复制图片失败", target, error);
    return safe_name.value();
}

Result<std::vector<std::string>> TdStore::list_images() const {
    auto present = path_exists(m_paths.images_dir);
    if (!present) return make_error(present.error().code, present.error().message);
    if (!present.value()) return std::vector<std::string>{};

    std::error_code error;
    std::vector<std::string> images;
    for (const auto &entry : std::filesystem::directory_iterator(m_paths.images_dir, error)) {
        if (error) return filesystem_error("遍历图片目录失败", m_paths.images_dir, error);
        std::error_code file_error;
        if (entry.is_regular_file(file_error)) images.push_back(entry.path().filename().u8string());
        if (file_error) return filesystem_error("检查图片文件失败", entry.path(), file_error);
    }
    if (error) return filesystem_error("遍历图片目录失败", m_paths.images_dir, error);
    std::sort(images.begin(), images.end());
    return images;
}

Result<bool> TdStore::delete_image(std::string name, bool force) const {
    auto safe_name = safe_file_name(std::move(name), "图片名称");
    if (!safe_name) return make_error(safe_name.error().code, safe_name.error().message);

    if (!force) {
        auto users = load_users();
        if (!users) return make_error(users.error().code, users.error().message);
        for (const auto &user : *users) {
            if (user.entrance_image == *safe_name || user.exit_image == *safe_name) {
                return make_error(ErrorCode::InvalidArgument, "图片仍被 TD 用户引用: " + *safe_name);
            }
        }
    }

    const auto target = m_paths.images_dir / *safe_name;
    auto present = path_exists(target);
    if (!present) return make_error(present.error().code, present.error().message);
    if (!present.value()) return false;
    auto regular = path_is_regular_file(target);
    if (!regular) return make_error(regular.error().code, regular.error().message);
    if (!regular.value()) return make_error(ErrorCode::InvalidArgument, "图片路径不是普通文件: " + *safe_name);

    std::error_code error;
    const bool removed = std::filesystem::remove(target, error);
    if (error) return filesystem_error("删除图片失败", target, error);
    return removed;
}

Result<std::filesystem::path> TdStore::image_path(std::string name) const {
    auto safe_name = safe_file_name(std::move(name), "图片名称");
    if (!safe_name) return make_error(safe_name.error().code, safe_name.error().message);
    return m_paths.images_dir / safe_name.value();
}

Result<void> TdStore::validate_user_references(const Model::Td::User &user) const {
    auto config = load_config();
    if (!config) return make_error(config.error().code, config.error().message);
    const auto has_machine = [&](int id) {
        return std::any_of(config->machines.begin(), config->machines.end(), [id](const Model::Td::Machine &machine) { return machine.id == id; });
    };
    if (!has_machine(user.entrance_machine_id)) return make_error(ErrorCode::InvalidArgument, "入口机器不存在: " + std::to_string(user.entrance_machine_id));
    if (!has_machine(user.exit_machine_id)) return make_error(ErrorCode::InvalidArgument, "出口机器不存在: " + std::to_string(user.exit_machine_id));

    for (const auto &image : {user.entrance_image, user.exit_image}) {
        auto safe_name = safe_file_name(image, "图片名称");
        if (!safe_name) return make_error(safe_name.error().code, safe_name.error().message);
        const auto path = m_paths.images_dir / safe_name.value();
        auto present = path_exists(path);
        if (!present) return make_error(present.error().code, present.error().message);
        if (!present.value()) return make_error(ErrorCode::InvalidArgument, "图片不存在: " + safe_name.value());
    }
    return Result<void>{};
}

} // namespace UBAANext
