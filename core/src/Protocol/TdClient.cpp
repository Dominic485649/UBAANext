#include <UBAANext/Protocol/TdClient.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace UBAANext::Protocol::Td {
namespace {

std::string uppercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    return value;
}

std::string json_string(const nlohmann::json &json, std::string_view key, std::string fallback = {}) {
    const auto it = json.find(std::string(key));
    if (it == json.end() || it->is_null()) return fallback;
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number_integer()) return std::to_string(it->get<long long>());
    if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
    if (it->is_boolean()) return *it ? "true" : "false";
    return fallback;
}

Result<Model::Td::Machine> machine_by_id(const Model::Td::Config &config, int machine_id) {
    const auto it = std::find_if(config.machines.begin(), config.machines.end(), [machine_id](const Model::Td::Machine &machine) {
        return machine.id == machine_id;
    });
    if (it == config.machines.end()) return make_error(ErrorCode::InvalidArgument, "TD 机器不存在: " + std::to_string(machine_id));
    return *it;
}

Result<nlohmann::json> parse_json_body(const ByteVector &body) {
    try {
        const std::string text(body.begin(), body.end());
        return nlohmann::json::parse(text);
    } catch (const nlohmann::json::exception &error) {
        return make_error(ErrorCode::ParseError, std::string("TD 响应 JSON 解析失败: ") + error.what());
    }
}

} // namespace

TdProtocolClient::TdProtocolClient(Model::Td::Config config, ITdTransport &transport)
    : m_config(std::move(config)), m_transport(transport) {}

Result<CheckResponse> TdProtocolClient::check(const Model::Td::User &user, int machine_id, std::int64_t timestamp_ms) {
    auto payload = build_check_data(m_config, user, machine_id, timestamp_ms);
    if (!payload) return make_error(payload.error().code, payload.error().message);
    const auto text = payload->dump();
    const ByteVector body(text.begin(), text.end());
    auto raw = request_json(body, check_request_type);
    if (!raw) return make_error(raw.error().code, raw.error().message);
    return parse_check_response(raw->payload);
}

Result<TdRawResponse> TdProtocolClient::upload_photo(int machine_id, const ByteVector &photo, std::int64_t timestamp_ms) {
    auto payload = build_photo_payload(m_config, machine_id, photo, timestamp_ms);
    if (!payload) return make_error(payload.error().code, payload.error().message);
    auto raw = request_json(payload.value(), photo_request_type);
    if (!raw) return make_error(raw.error().code, raw.error().message);
    if (raw->status != "success") return make_error(ErrorCode::ParseError, "照片上传失败: " + raw->status);
    return raw.value();
}

Result<int> TdProtocolClient::query_count(const Model::Td::User &user, std::optional<int> machine_id, std::int64_t timestamp_ms) {
    auto response = check(user, machine_id.value_or(user.entrance_machine_id), timestamp_ms);
    if (!response) return make_error(response.error().code, response.error().message);
    if (!response->count) return make_error(ErrorCode::ParseError, "服务器消息中没有锻炼次数: " + response->server_message);
    return response->count.value();
}

Result<TdRawResponse> TdProtocolClient::request_json(const ByteVector &body, std::uint8_t request_type) const {
    auto frame = encode_frame(request_type, body);
    if (!frame) return make_error(frame.error().code, frame.error().message);

    TdEndpoint endpoint;
    endpoint.ip = m_config.server.ip;
    endpoint.port = m_config.server.port;
    endpoint.timeout_seconds = m_config.server.timeout_seconds;

    auto response_frame = m_transport.exchange(endpoint, frame.value());
    if (!response_frame) return make_error(response_frame.error().code, response_frame.error().message);
    auto decoded = decode_frame(response_frame.value());
    if (!decoded) return make_error(decoded.error().code, decoded.error().message);
    if (decoded->header.request_type != request_type) {
        return make_error(ErrorCode::ParseError,
                          "TD 响应类型不匹配: expected " + std::to_string(request_type) + ", got " + std::to_string(decoded->header.request_type));
    }
    if (decoded->header.length == 0) return make_error(ErrorCode::ParseError, "TD 响应为空");
    auto json = parse_json_body(decoded->body);
    if (!json) return make_error(json.error().code, json.error().message);
    return parse_raw_response(json.value());
}

Result<ByteVector> encode_frame(std::uint8_t request_type, const ByteVector &body) {
    if (body.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        return make_error(ErrorCode::InvalidArgument, "TD 请求体过大");
    }
    const auto length = static_cast<std::uint32_t>(body.size());
    ByteVector frame;
    frame.reserve(5 + body.size());
    frame.push_back(static_cast<std::uint8_t>((length >> 24U) & 0xffU));
    frame.push_back(static_cast<std::uint8_t>((length >> 16U) & 0xffU));
    frame.push_back(static_cast<std::uint8_t>((length >> 8U) & 0xffU));
    frame.push_back(static_cast<std::uint8_t>(length & 0xffU));
    frame.push_back(request_type);
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

Result<FrameHeader> decode_frame_header(const ByteVector &header) {
    if (header.size() != 5) return make_error(ErrorCode::ParseError, "TD 响应头长度必须为 5 字节");
    FrameHeader decoded;
    decoded.length = (static_cast<std::uint32_t>(header[0]) << 24U) |
                     (static_cast<std::uint32_t>(header[1]) << 16U) |
                     (static_cast<std::uint32_t>(header[2]) << 8U) |
                     static_cast<std::uint32_t>(header[3]);
    if (decoded.length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        return make_error(ErrorCode::ParseError, "TD 响应长度非法");
    }
    decoded.request_type = header[4];
    return decoded;
}

Result<Frame> decode_frame(const ByteVector &frame) {
    if (frame.size() < 5) return make_error(ErrorCode::ParseError, "TD 响应帧过短");
    const ByteVector header_bytes(frame.begin(), frame.begin() + 5);
    auto header = decode_frame_header(header_bytes);
    if (!header) return make_error(header.error().code, header.error().message);
    const auto expected_size = 5U + static_cast<std::size_t>(header->length);
    if (frame.size() != expected_size) {
        return make_error(ErrorCode::ParseError, "TD 响应长度不匹配");
    }
    Frame decoded;
    decoded.header = header.value();
    decoded.body.assign(frame.begin() + 5, frame.end());
    return decoded;
}

Result<nlohmann::json> build_check_data(const Model::Td::Config &config,
                                        const Model::Td::User &user,
                                        int machine_id,
                                        std::int64_t timestamp_ms) {
    if (timestamp_ms < 0) return make_error(ErrorCode::InvalidArgument, "timestamp_ms 不能为负数");
    auto machine = machine_by_id(config, machine_id);
    if (!machine) return make_error(machine.error().code, machine.error().message);
    if (user.student_id.empty()) return make_error(ErrorCode::InvalidArgument, "student_id 不能为空");
    if (user.card_id.empty()) return make_error(ErrorCode::InvalidArgument, "card_id 不能为空");
    if (config.school_number.empty()) return make_error(ErrorCode::InvalidArgument, "schoolno 不能为空");
    if (config.event_number.empty()) return make_error(ErrorCode::InvalidArgument, "eventno 不能为空");

    return nlohmann::json{{"cardno", uppercase_ascii(user.card_id)},
                          {"userno", uppercase_ascii(user.student_id)},
                          {"timestamp", std::to_string(timestamp_ms)},
                          {"type", config.type},
                          {"eventno", config.event_number},
                          {"ln", std::to_string(machine->id)},
                          {"sn", machine->serial_number},
                          {"schoolno", config.school_number}};
}

Result<ByteVector> build_photo_payload(const Model::Td::Config &config,
                                       int machine_id,
                                       const ByteVector &photo,
                                       std::int64_t timestamp_ms) {
    if (timestamp_ms < 0) return make_error(ErrorCode::InvalidArgument, "timestamp_ms 不能为负数");
    auto machine = machine_by_id(config, machine_id);
    if (!machine) return make_error(machine.error().code, machine.error().message);
    const auto prefix = machine->serial_number + "_" + std::to_string(timestamp_ms);
    ByteVector payload(prefix.begin(), prefix.end());
    payload.insert(payload.end(), photo.begin(), photo.end());
    return payload;
}

std::string clean_server_message(std::string message) {
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.front())) != 0) message.erase(message.begin());
    while (!message.empty() && std::isspace(static_cast<unsigned char>(message.back())) != 0) message.pop_back();
    std::string cleaned;
    cleaned.reserve(message.size());
    for (std::size_t i = 0; i < message.size(); ++i) {
        if (message[i] == '\r') continue;
        if (message[i] == '\n') {
            while (!cleaned.empty() && cleaned.back() == ' ') cleaned.pop_back();
            cleaned += ", ";
            while (i + 1 < message.size() && std::isspace(static_cast<unsigned char>(message[i + 1])) != 0) ++i;
        } else {
            cleaned.push_back(message[i]);
        }
    }
    return cleaned;
}

std::optional<int> extract_exercise_count(const std::string &message) {
    const std::string marker = "本学期锻炼次数";
    const auto marker_pos = message.find(marker);
    if (marker_pos == std::string::npos) return std::nullopt;

    auto cursor = marker_pos + marker.size();
    while (cursor < message.size() && std::isspace(static_cast<unsigned char>(message[cursor])) != 0) ++cursor;
    if (cursor >= message.size()) return std::nullopt;

    if (message[cursor] == ':') {
        ++cursor;
    } else if (message.compare(cursor, std::string("：").size(), "：") == 0) {
        cursor += std::string("：").size();
    } else {
        return std::nullopt;
    }

    while (cursor < message.size() && std::isspace(static_cast<unsigned char>(message[cursor])) != 0) ++cursor;
    const auto begin = cursor;
    while (cursor < message.size() && std::isdigit(static_cast<unsigned char>(message[cursor])) != 0) ++cursor;
    if (begin == cursor) return std::nullopt;

    int count = 0;
    const auto *first = message.data() + begin;
    const auto *last = message.data() + cursor;
    const auto result = std::from_chars(first, last, count);
    if (result.ec != std::errc{} || result.ptr != last) return std::nullopt;
    return count;
}

Result<TdRawResponse> parse_raw_response(const nlohmann::json &json) {
    if (!json.is_object()) return make_error(ErrorCode::ParseError, "TD 响应必须是对象");
    TdRawResponse response;
    response.status = json_string(json, "status");
    if (response.status.empty()) return make_error(ErrorCode::ParseError, "TD 响应缺少 status");
    response.server_message = clean_server_message(json_string(json, "srvresp"));
    response.payload = json;
    return response;
}

Result<CheckResponse> parse_check_response(const nlohmann::json &json) {
    auto raw = parse_raw_response(json);
    if (!raw) return make_error(raw.error().code, raw.error().message);
    if (raw->status != "success") return make_error(ErrorCode::ParseError, "打卡请求失败: " + raw->status);
    CheckResponse response;
    response.server_message = raw->server_message;
    response.success = json_string(json, "srvresp").find("成功") != std::string::npos;
    response.count = extract_exercise_count(response.server_message);
    response.payload = raw->payload;
    return response;
}

} // namespace UBAANext::Protocol::Td
