#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Td.hpp>
#include <UBAANext/Platform/AppDataPathProvider.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace UBAANext {

struct TdStorePaths {
    std::filesystem::path root;
    std::filesystem::path images_dir;
    std::filesystem::path logs_dir;
    std::filesystem::path config_path;
    std::filesystem::path users_path;
    std::filesystem::path settings_path;
    std::filesystem::path state_path;
};

class TdStore {
public:
    explicit TdStore(std::filesystem::path root);

    [[nodiscard]] static Result<TdStore> from_app_data_dir(const IAppDataPathProvider &provider);

    [[nodiscard]] const TdStorePaths &paths() const noexcept;

    [[nodiscard]] Result<void> initialize() const;

    [[nodiscard]] Result<Model::Td::Config> load_config() const;
    [[nodiscard]] Result<void> save_config(const Model::Td::Config &config) const;

    [[nodiscard]] Result<std::vector<Model::Td::User>> load_users() const;
    [[nodiscard]] Result<std::optional<Model::Td::User>> load_user(const std::string &student_id) const;
    [[nodiscard]] Result<void> save_user(const Model::Td::User &user, bool allow_update = true) const;
    [[nodiscard]] Result<bool> delete_user(const std::string &student_id) const;

    [[nodiscard]] Result<std::vector<Model::Td::UserState>> load_states() const;
    [[nodiscard]] Result<std::optional<Model::Td::UserState>> load_state(const std::string &student_id) const;
    [[nodiscard]] Result<void> save_states(const std::vector<Model::Td::UserState> &states) const;
    [[nodiscard]] Result<void> save_state(const Model::Td::UserState &state) const;

    [[nodiscard]] Result<std::string> add_image(const std::filesystem::path &source,
                                                std::string name = {},
                                                bool overwrite = false) const;
    [[nodiscard]] Result<std::vector<std::string>> list_images() const;
    [[nodiscard]] Result<std::filesystem::path> image_path(std::string name) const;

private:
    [[nodiscard]] Result<void> validate_user_references(const Model::Td::User &user) const;

    TdStorePaths m_paths;
};

} // namespace UBAANext
