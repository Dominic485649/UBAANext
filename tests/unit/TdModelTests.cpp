#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Model/Td.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace td = UBAANext::Model::Td;
namespace um = UBAANext;

TEST_CASE("TD 默认配置对齐 AutoTD 核心常量", "[Td][Model]") {
    const auto config = td::default_config();

    CHECK(td::completion_limit == 32);
    CHECK(config.type == 1);
    CHECK(config.school_number == "10006");
    CHECK(config.event_number == "802");
    CHECK(config.server.ip == "10.212.28.38");
    CHECK(config.server.port == td::default_port);
    CHECK(config.server.timeout_seconds == td::default_timeout_seconds);
    REQUIRE(config.windows.size() == 3);
    CHECK(config.windows[0] == "07:30-10:00");
    CHECK(config.poll_seconds == td::default_poll_seconds);
    REQUIRE(config.machines.size() == 12);
    CHECK(config.machines.front().id == 2);
    CHECK(config.machines.back().id == 13);
}

TEST_CASE("TD 用户从学号推导 card_id 并保留图片文件名", "[Td][Model]") {
    const auto user = td::make_user("2023123456", "", 8, 11, "C:/tmp/in.png", "/tmp/out.jpg");

    REQUIRE(user);
    CHECK(user->student_id == "2023123456");
    CHECK(user->card_id == "78966A00");
    CHECK(user->entrance_machine_id == 8);
    CHECK(user->exit_machine_id == 11);
    CHECK(user->entrance_image == "in.png");
    CHECK(user->exit_image == "out.jpg");
    CHECK(user->rounds == td::default_rounds);
    CHECK(user->wait_time_min_minutes == td::default_wait_time_min_minutes);
    CHECK(user->wait_time_max_minutes == td::default_wait_time_max_minutes);
}

TEST_CASE("TD 用户构造拒绝非法字段", "[Td][Model]") {
    auto missing_student = td::make_user("", "", 8, 11, "in.png", "out.jpg");
    REQUIRE_FALSE(missing_student);
    CHECK(missing_student.error().code == um::ErrorCode::InvalidArgument);

    auto non_numeric_student = td::make_user("BUAA", "", 8, 11, "in.png", "out.jpg");
    REQUIRE_FALSE(non_numeric_student);
    CHECK(non_numeric_student.error().code == um::ErrorCode::InvalidArgument);

    auto bad_wait = td::make_user("2023123456", "", 8, 11, "in.png", "out.jpg", 3, 240, 180);
    REQUIRE_FALSE(bad_wait);
    CHECK(bad_wait.error().code == um::ErrorCode::InvalidArgument);

    auto negative_count = td::make_user("2023123456", "", 8, 11, "in.png", "out.jpg", 3, 180, 240, -1);
    REQUIRE_FALSE(negative_count);
    CHECK(negative_count.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TD quick 校区别名与机器池保持确定性", "[Td][Model]") {
    const auto config = td::default_config();

    REQUIRE(td::normalize_campus("沙河"));
    CHECK(td::normalize_campus("沙河").value() == "shahe");
    REQUIRE(td::normalize_campus("SH"));
    CHECK(td::normalize_campus("SH").value() == "shahe");
    REQUIRE(td::normalize_campus("本部"));
    CHECK(td::normalize_campus("本部").value() == "xueyuanlu");
    REQUIRE(td::normalize_campus("xyl"));
    CHECK(td::normalize_campus("xyl").value() == "xueyuanlu");

    const auto shahe_entrances = td::machine_pool(config.machines, "shahe", true);
    const auto shahe_exits = td::machine_pool(config.machines, "shahe", false);
    const auto xyl_entrances = td::machine_pool(config.machines, "xueyuanlu", true);
    const auto xyl_exits = td::machine_pool(config.machines, "xueyuanlu", false);

    REQUIRE(shahe_entrances.size() == 3);
    CHECK(shahe_entrances[0].id == 8);
    CHECK(shahe_entrances[2].id == 10);
    REQUIRE(shahe_exits.size() == 3);
    CHECK(shahe_exits[0].id == 11);
    CHECK(shahe_exits[2].id == 13);
    REQUIRE(xyl_entrances.size() == 3);
    CHECK(xyl_entrances[0].id == 2);
    CHECK(xyl_entrances[2].id == 4);
    REQUIRE(xyl_exits.size() == 3);
    CHECK(xyl_exits[0].id == 5);
    CHECK(xyl_exits[2].id == 7);

    auto unknown = td::normalize_campus("未知校区");
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TD quick 用户选择机器和图片可重复", "[Td][Model]") {
    const auto config = td::default_config();
    const std::vector<std::string> images = {"a.jpg", "b.jpg", "c.jpg"};

    const auto user = td::build_quick_user(config, images, "2023123456", "shahe", 4, 5, 1, 2);

    REQUIRE(user);
    CHECK(user->entrance_machine_id == 9);
    CHECK(user->exit_machine_id == 13);
    CHECK(user->entrance_image == "b.jpg");
    CHECK(user->exit_image == "c.jpg");
    CHECK(user->card_id == "78966A00");

    auto no_images = td::build_quick_user(config, {}, "2023123456", "shahe");
    REQUIRE_FALSE(no_images);
    CHECK(no_images.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TD 用户 JSON 支持往返和 AutoTD legacy 图片字段", "[Td][Model]") {
    const auto user = td::make_user("2023123456", "abcd", 8, 11, "in.png", "out.jpg", 2, 181, 239, 7);
    REQUIRE(user);

    const auto json = td::user_to_json(user.value());
    CHECK(json["student_id"] == "2023123456");
    CHECK(json["card_id"] == "ABCD");
    CHECK(json["cached_term_count"] == 7);

    const auto parsed = td::user_from_json(json);
    REQUIRE(parsed);
    CHECK(parsed->student_id == user->student_id);
    CHECK(parsed->card_id == user->card_id);
    CHECK(parsed->entrance_machine_id == user->entrance_machine_id);
    CHECK(parsed->exit_machine_id == user->exit_machine_id);
    CHECK(parsed->cached_term_count == 7);

    const nlohmann::json legacy{{"student_id", "2023123456"},
                                {"entrance_machine_id", 8},
                                {"exit_machine_id", 11},
                                {"entrance_photo_path", "legacy-in.png"},
                                {"exit_photo_path", "legacy-out.png"}};
    const auto legacy_user = td::user_from_json(legacy);
    REQUIRE(legacy_user);
    CHECK(legacy_user->entrance_image == "legacy-in.png");
    CHECK(legacy_user->exit_image == "legacy-out.png");
}

TEST_CASE("TD 配置 JSON 兼容 AutoTD config 字段", "[Td][Model]") {
    const nlohmann::json json{{"type", "1"},
                              {"schoolno", 10006},
                              {"eventno", "802"},
                              {"server", {{"ip", "10.0.0.1"}, {"port", "8888"}, {"timeout", 11}}},
                              {"windows", {"08:00-09:00"}},
                              {"poll_seconds", "30"},
                              {"machine", {{{"id", 1}, {"machinesn", "IN"}, {"location", "北航沙河TD入口"}, {"doortype", "1"}},
                                           {{"id", 2}, {"machinesn", "OUT"}, {"location", "北航沙河TD出口"}, {"doortype", "2"}}}}};

    const auto config = td::config_from_json(json);
    REQUIRE(config);
    CHECK(config->type == 1);
    CHECK(config->school_number == "10006");
    CHECK(config->event_number == "802");
    CHECK(config->server.ip == "10.0.0.1");
    CHECK(config->server.port == 8888);
    CHECK(config->server.timeout_seconds == 11);
    REQUIRE(config->windows.size() == 1);
    CHECK(config->windows[0] == "08:00-09:00");
    CHECK(config->poll_seconds == 30);
    REQUIRE(config->machines.size() == 2);
    CHECK(config->machines[0].serial_number == "IN");

    const auto round_trip = td::config_from_json(td::config_to_json(config.value()));
    REQUIRE(round_trip);
    CHECK(round_trip->server.ip == config->server.ip);
    CHECK(round_trip->machines.size() == config->machines.size());
}

TEST_CASE("TD 状态 JSON 校验轮次和次数", "[Td][Model]") {
    td::UserState state;
    state.student_id = "2023123456";
    state.date = "2026-06-02";
    state.status = "waiting";
    state.next_action = "exit";
    state.completed_rounds = 1;
    state.term_count = 12;
    state.next_run_at = "2026-06-02T12:00:00";

    const auto parsed = td::state_from_json(td::state_to_json(state));
    REQUIRE(parsed);
    CHECK(parsed->student_id == state.student_id);
    CHECK(parsed->status == "waiting");
    CHECK(parsed->next_action == "exit");
    CHECK(parsed->completed_rounds == 1);
    CHECK(parsed->term_count == 12);

    auto negative_rounds = td::state_from_json(nlohmann::json{{"student_id", "2023123456"}, {"completed_rounds", -1}});
    REQUIRE_FALSE(negative_rounds);
    CHECK(negative_rounds.error().code == um::ErrorCode::InvalidArgument);

    auto negative_count = td::state_from_json(nlohmann::json{{"student_id", "2023123456"}, {"term_count", -1}});
    REQUIRE_FALSE(negative_count);
    CHECK(negative_count.error().code == um::ErrorCode::InvalidArgument);
}
