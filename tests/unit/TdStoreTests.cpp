#include <UBAANext/Storage/TdStore.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace td = UBAANext::Model::Td;
namespace um = UBAANext;

namespace {

std::filesystem::path make_temp_root(const std::string &name) {
    auto root = std::filesystem::temp_directory_path() / ("ubaanext-td-store-" + name);
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

class StaticAppDataPathProvider : public um::IAppDataPathProvider {
public:
    explicit StaticAppDataPathProvider(std::filesystem::path path) : m_path(std::move(path)) {}

    um::Result<std::filesystem::path> app_data_dir() const override { return m_path; }

private:
    std::filesystem::path m_path;
};

} // namespace

TEST_CASE("TdStore 从应用数据目录建立 TD 命名空间", "[Td][Store]") {
    const auto app_data = make_temp_root("provider");
    StaticAppDataPathProvider provider(app_data);

    const auto store = um::TdStore::from_app_data_dir(provider);

    REQUIRE(store);
    CHECK(store->paths().root == app_data / "td");
    CHECK(store->paths().images_dir == app_data / "td" / "images");
}

TEST_CASE("TdStore 初始化创建目录和默认文件且不覆盖已有配置", "[Td][Store]") {
    const auto root = make_temp_root("init");
    um::TdStore store(root);
    write_text(root / "config.json", "{\"type\":1,\"schoolno\":\"keep\",\"eventno\":\"802\",\"server\":{\"ip\":\"1.2.3.4\",\"port\":8888,\"timeout\":10},\"machine\":[{\"id\":8,\"machinesn\":\"IN\",\"location\":\"北航沙河TD入口\",\"doortype\":\"1\"}]}\n");

    const auto initialized = store.initialize();

    REQUIRE(initialized);
    CHECK(std::filesystem::is_directory(store.paths().images_dir));
    CHECK(std::filesystem::is_directory(store.paths().logs_dir));
    CHECK(read_text(store.paths().config_path).find("keep") != std::string::npos);
    CHECK(std::filesystem::exists(store.paths().users_path));
    CHECK(std::filesystem::exists(store.paths().settings_path));
    CHECK(std::filesystem::exists(store.paths().state_path));
}

TEST_CASE("TdStore 保存和加载配置", "[Td][Store]") {
    const auto root = make_temp_root("config");
    um::TdStore store(root);
    REQUIRE(store.initialize());

    auto config = td::default_config();
    config.server.ip = "10.0.0.2";
    config.poll_seconds = 90;

    REQUIRE(store.save_config(config));
    const auto loaded = store.load_config();

    REQUIRE(loaded);
    CHECK(loaded->server.ip == "10.0.0.2");
    CHECK(loaded->poll_seconds == 90);
}

TEST_CASE("TdStore 图片添加、列出和覆盖检查", "[Td][Store]") {
    const auto root = make_temp_root("images");
    um::TdStore store(root);
    REQUIRE(store.initialize());
    const auto source = root / "source.jpg";
    write_text(source, "image-v1");

    const auto added = store.add_image(source, "custom.jpg");
    REQUIRE(added);
    CHECK(added.value() == "custom.jpg");
    CHECK(read_text(store.paths().images_dir / "custom.jpg") == "image-v1");

    const auto images = store.list_images();
    REQUIRE(images);
    REQUIRE(images->size() == 1);
    CHECK((*images)[0] == "custom.jpg");

    const auto duplicate = store.add_image(source, "custom.jpg");
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().code == um::ErrorCode::InvalidArgument);

    write_text(source, "image-v2");
    REQUIRE(store.add_image(source, "custom.jpg", true));
    CHECK(read_text(store.paths().images_dir / "custom.jpg") == "image-v2");
}

TEST_CASE("TdStore 图片添加拒绝无效路径", "[Td][Store]") {
    const auto root = make_temp_root("bad-image");
    um::TdStore store(root);
    REQUIRE(store.initialize());

    auto missing = store.add_image(root / "missing.jpg");
    REQUIRE_FALSE(missing);
    CHECK(missing.error().code == um::ErrorCode::InvalidArgument);

    const auto source = root / "source.jpg";
    write_text(source, "image");
    auto nested_name = store.add_image(source, "nested/custom.jpg");
    REQUIRE_FALSE(nested_name);
    CHECK(nested_name.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TdStore 保存用户时校验机器和图片引用", "[Td][Store]") {
    const auto root = make_temp_root("users");
    um::TdStore store(root);
    REQUIRE(store.initialize());
    const auto source = root / "source.jpg";
    write_text(source, "image");
    REQUIRE(store.add_image(source, "td.jpg"));

    const auto user = td::make_user("2023123456", "", 8, 11, "td.jpg", "td.jpg");
    REQUIRE(user);
    REQUIRE(store.save_user(user.value(), false));

    const auto loaded = store.load_user("2023123456");
    REQUIRE(loaded);
    REQUIRE(loaded->has_value());
    CHECK((*loaded)->student_id == "2023123456");

    const auto duplicate = store.save_user(user.value(), false);
    REQUIRE_FALSE(duplicate);
    CHECK(duplicate.error().code == um::ErrorCode::InvalidArgument);

    const auto bad_machine = td::make_user("2023000000", "", 999, 11, "td.jpg", "td.jpg");
    REQUIRE(bad_machine);
    auto saved_bad_machine = store.save_user(bad_machine.value());
    REQUIRE_FALSE(saved_bad_machine);
    CHECK(saved_bad_machine.error().code == um::ErrorCode::InvalidArgument);

    const auto missing_image = td::make_user("2023000001", "", 8, 11, "missing.jpg", "td.jpg");
    REQUIRE(missing_image);
    auto saved_missing_image = store.save_user(missing_image.value());
    REQUIRE_FALSE(saved_missing_image);
    CHECK(saved_missing_image.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TdStore 删除用户并按学号排序列出", "[Td][Store]") {
    const auto root = make_temp_root("delete-user");
    um::TdStore store(root);
    REQUIRE(store.initialize());
    const auto source = root / "source.jpg";
    write_text(source, "image");
    REQUIRE(store.add_image(source, "td.jpg"));

    const auto later = td::make_user("2023123457", "", 8, 11, "td.jpg", "td.jpg");
    const auto earlier = td::make_user("2023123456", "", 8, 11, "td.jpg", "td.jpg");
    REQUIRE(later);
    REQUIRE(earlier);
    REQUIRE(store.save_user(later.value()));
    REQUIRE(store.save_user(earlier.value()));

    auto users = store.load_users();
    REQUIRE(users);
    REQUIRE(users->size() == 2);
    CHECK((*users)[0].student_id == "2023123456");
    CHECK((*users)[1].student_id == "2023123457");

    const auto removed = store.delete_user("2023123456");
    REQUIRE(removed);
    CHECK(removed.value());
    users = store.load_users();
    REQUIRE(users);
    REQUIRE(users->size() == 1);
    CHECK((*users)[0].student_id == "2023123457");
}

TEST_CASE("TdStore 保存和加载用户状态", "[Td][Store]") {
    const auto root = make_temp_root("states");
    um::TdStore store(root);
    REQUIRE(store.initialize());

    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-02";
    state.status = "waiting";
    state.next_action = "exit";
    state.completed_rounds = 1;
    state.term_count = 12;

    REQUIRE(store.save_state(state));
    const auto loaded = store.load_state("2023123456");
    REQUIRE(loaded);
    REQUIRE(loaded->has_value());
    CHECK((*loaded)->status == "waiting");
    CHECK((*loaded)->term_count == 12);

    auto states = store.load_states();
    REQUIRE(states);
    REQUIRE(states->size() == 1);
    CHECK((*states)[0].student_id == "2023123456");
}

TEST_CASE("TdStore 删除图片处理不存在、引用、force 和路径穿越", "[Td][Store][ImageDelete]") {
    const auto root = make_temp_root("delete-image");
    um::TdStore store(root);
    REQUIRE(store.initialize());
    const auto source = root / "source.jpg";
    write_text(source, "image");
    REQUIRE(store.add_image(source, "td.jpg"));

    const auto missing = store.delete_image("missing.jpg");
    REQUIRE(missing);
    CHECK_FALSE(*missing);

    const auto traversal = store.delete_image("../td.jpg", true);
    REQUIRE_FALSE(traversal);
    CHECK(traversal.error().code == um::ErrorCode::InvalidArgument);

    const auto user = td::make_user("2023123456", "", 8, 11, "td.jpg", "td.jpg");
    REQUIRE(user);
    REQUIRE(store.save_user(*user));

    const auto referenced = store.delete_image("td.jpg");
    REQUIRE_FALSE(referenced);
    CHECK(referenced.error().code == um::ErrorCode::InvalidArgument);
    CHECK(std::filesystem::exists(store.paths().images_dir / "td.jpg"));

    const auto forced = store.delete_image("td.jpg", true);
    REQUIRE(forced);
    CHECK(*forced);
    CHECK_FALSE(std::filesystem::exists(store.paths().images_dir / "td.jpg"));
}
