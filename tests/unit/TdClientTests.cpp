#include <UBAANext/Protocol/TdClient.hpp>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace proto = UBAANext::Protocol::Td;
namespace td = UBAANext::Model::Td;
namespace um = UBAANext;

namespace {

class MockTdTransport : public proto::ITdTransport {
public:
    um::Result<proto::ByteVector> exchange(const proto::TdEndpoint &endpoint, const proto::ByteVector &request_frame) override {
        last_endpoint = endpoint;
        last_request_frame = request_frame;
        if (!next_response) return um::make_error(um::ErrorCode::NetworkError, "mock transport failed");
        return next_response.value();
    }

    proto::TdEndpoint last_endpoint;
    proto::ByteVector last_request_frame;
    std::optional<proto::ByteVector> next_response;
};

proto::ByteVector json_frame(std::uint8_t request_type, const nlohmann::json &json) {
    const auto text = json.dump();
    proto::ByteVector body(text.begin(), text.end());
    auto frame = proto::encode_frame(request_type, body);
    REQUIRE(frame);
    return frame.value();
}

} // namespace

TEST_CASE("TD 协议帧使用 big-endian 长度和请求类型", "[Td][Protocol]") {
    const proto::ByteVector body{'a', 'b', 'c'};

    const auto frame = proto::encode_frame(proto::check_request_type, body);

    REQUIRE(frame);
    REQUIRE(frame->size() == 8);
    CHECK((*frame)[0] == 0);
    CHECK((*frame)[1] == 0);
    CHECK((*frame)[2] == 0);
    CHECK((*frame)[3] == 3);
    CHECK((*frame)[4] == proto::check_request_type);
    CHECK((*frame)[5] == 'a');

    const auto header = proto::decode_frame_header({0, 0, 1, 0, proto::photo_request_type});
    REQUIRE(header);
    CHECK(header->length == 256);
    CHECK(header->request_type == proto::photo_request_type);

    const auto decoded = proto::decode_frame(frame.value());
    REQUIRE(decoded);
    CHECK(decoded->header.length == 3);
    CHECK(decoded->header.request_type == proto::check_request_type);
    CHECK(decoded->body == body);
}

TEST_CASE("TD 协议帧拒绝非法响应", "[Td][Protocol]") {
    auto short_header = proto::decode_frame_header({0, 1});
    REQUIRE_FALSE(short_header);
    CHECK(short_header.error().code == um::ErrorCode::ParseError);

    auto short_frame = proto::decode_frame({0, 0, 0, 3, proto::check_request_type, 'x'});
    REQUIRE_FALSE(short_frame);
    CHECK(short_frame.error().code == um::ErrorCode::ParseError);

    auto empty_response = proto::decode_frame({0, 0, 0, 0, proto::check_request_type});
    REQUIRE(empty_response);
    CHECK(empty_response->body.empty());
}

TEST_CASE("TD checkdata 构造对齐 AutoTD 字段", "[Td][Protocol]") {
    const auto config = td::default_config();
    const auto user = td::make_user("2023123456", "abcd", 8, 11, "in.jpg", "out.jpg");
    REQUIRE(user);

    const auto payload = proto::build_check_data(config, user.value(), 8, 1717200000123);

    REQUIRE(payload);
    CHECK((*payload)["cardno"] == "ABCD");
    CHECK((*payload)["userno"] == "2023123456");
    CHECK((*payload)["timestamp"] == "1717200000123");
    CHECK((*payload)["type"] == config.type);
    CHECK((*payload)["eventno"] == config.event_number);
    CHECK((*payload)["ln"] == "8");
    CHECK((*payload)["sn"] == "20210511001");
    CHECK((*payload)["schoolno"] == config.school_number);

    const auto missing_machine = proto::build_check_data(config, user.value(), 999, 1717200000123);
    REQUIRE_FALSE(missing_machine);
    CHECK(missing_machine.error().code == um::ErrorCode::InvalidArgument);
}

TEST_CASE("TD 照片 payload 使用机器序列号和毫秒时间戳前缀", "[Td][Protocol]") {
    const proto::ByteVector photo{'P', 'N', 'G'};

    const auto payload = proto::build_photo_payload(td::default_config(), 8, photo, 1717200000123);

    REQUIRE(payload);
    const std::string text(payload->begin(), payload->end());
    CHECK(text == "20210511001_1717200000123PNG");
}

TEST_CASE("TD 服务器消息清理并提取本学期锻炼次数", "[Td][Protocol]") {
    const auto cleaned = proto::clean_server_message("  打卡成功\n \n本学期锻炼次数：31\n ");

    CHECK(cleaned == "打卡成功, 本学期锻炼次数：31");
    REQUIRE(proto::extract_exercise_count(cleaned));
    CHECK(proto::extract_exercise_count(cleaned).value() == 31);
    CHECK_FALSE(proto::extract_exercise_count("没有次数"));
}

TEST_CASE("TD check 响应解析识别成功和次数", "[Td][Protocol]") {
    const nlohmann::json json{{"status", "success"}, {"srvresp", "打卡成功\n本学期锻炼次数: 32"}};

    const auto response = proto::parse_check_response(json);

    REQUIRE(response);
    CHECK(response->success);
    CHECK(response->server_message == "打卡成功, 本学期锻炼次数: 32");
    REQUIRE(response->count);
    CHECK(response->count.value() == 32);

    const auto failed = proto::parse_check_response(nlohmann::json{{"status", "failed"}, {"srvresp", "拒绝"}});
    REQUIRE_FALSE(failed);
    CHECK(failed.error().code == um::ErrorCode::ParseError);
}

TEST_CASE("TD 协议客户端通过 mock transport 发送 check 请求", "[Td][Protocol]") {
    MockTdTransport transport;
    transport.next_response = json_frame(proto::check_request_type, nlohmann::json{{"status", "success"}, {"srvresp", "成功 本学期锻炼次数：10"}});
    proto::TdProtocolClient client(td::default_config(), transport);
    const auto user = td::make_user("2023123456", "abcd", 8, 11, "in.jpg", "out.jpg");
    REQUIRE(user);

    const auto response = client.check(user.value(), 8, 1717200000123);

    REQUIRE(response);
    CHECK(response->success);
    REQUIRE(response->count);
    CHECK(response->count.value() == 10);
    CHECK(transport.last_endpoint.ip == "10.212.28.38");
    CHECK(transport.last_endpoint.port == 8888);

    const auto sent = proto::decode_frame(transport.last_request_frame);
    REQUIRE(sent);
    CHECK(sent->header.request_type == proto::check_request_type);
    const auto body = nlohmann::json::parse(std::string(sent->body.begin(), sent->body.end()));
    CHECK(body["ln"] == "8");
    CHECK(body["sn"] == "20210511001");
}

TEST_CASE("TD 协议客户端校验响应类型、上传状态和查询次数", "[Td][Protocol]") {
    MockTdTransport transport;
    proto::TdProtocolClient client(td::default_config(), transport);
    const auto user = td::make_user("2023123456", "abcd", 8, 11, "in.jpg", "out.jpg");
    REQUIRE(user);

    transport.next_response = json_frame(proto::photo_request_type, nlohmann::json{{"status", "success"}, {"srvresp", "ok"}});
    const auto mismatch = client.check(user.value(), 8, 1717200000123);
    REQUIRE_FALSE(mismatch);
    CHECK(mismatch.error().code == um::ErrorCode::ParseError);

    transport.next_response = json_frame(proto::photo_request_type, nlohmann::json{{"status", "failed"}, {"srvresp", "拒绝"}});
    const proto::ByteVector one_byte_photo{'x'};
    const auto upload_failed = client.upload_photo(8, one_byte_photo, 1717200000123);
    REQUIRE_FALSE(upload_failed);
    CHECK(upload_failed.error().code == um::ErrorCode::ParseError);

    transport.next_response = json_frame(proto::check_request_type, nlohmann::json{{"status", "success"}, {"srvresp", "成功 本学期锻炼次数：12"}});
    const auto count = client.query_count(user.value(), std::nullopt, 1717200000123);
    REQUIRE(count);
    CHECK(count.value() == 12);
}
