#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Model/Td.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace UBAANext::Protocol::Td {

constexpr std::uint8_t check_request_type = 80;
constexpr std::uint8_t photo_request_type = 100;

using ByteVector = std::vector<std::uint8_t>;

struct FrameHeader {
    std::uint32_t length = 0;
    std::uint8_t request_type = 0;
};

struct Frame {
    FrameHeader header;
    ByteVector body;
};

struct TdEndpoint {
    std::string ip;
    int port = Model::Td::default_port;
    int timeout_seconds = Model::Td::default_timeout_seconds;
};

struct TdRawResponse {
    std::string status;
    std::string server_message;
    nlohmann::json payload;
};

struct CheckResponse {
    bool success = false;
    std::string server_message;
    std::optional<int> count;
    nlohmann::json payload;
};

class ITdTransport {
public:
    virtual ~ITdTransport() = default;

    /** WriteGated transport boundary: production implementations may contact the TD server; tests must inject a mock transport. */
    [[nodiscard]] virtual Result<ByteVector> exchange(const TdEndpoint &endpoint, const ByteVector &request_frame) = 0;
};

class ITdClient {
public:
    virtual ~ITdClient() = default;

    /** WriteGated remote mutation: callers must confirm before invoking a production client against the real TD server. */
    [[nodiscard]] virtual Result<CheckResponse> check(const Model::Td::User &user,
                                                      int machine_id,
                                                      std::int64_t timestamp_ms) = 0;
    /** WriteGated remote mutation: callers must confirm before uploading real user photos. */
    [[nodiscard]] virtual Result<TdRawResponse> upload_photo(int machine_id,
                                                             const ByteVector &photo,
                                                             std::int64_t timestamp_ms) = 0;
    /** Query uses the same TD check protocol but must be routed through explicit caller policy because the server-side semantics are write-like. */
    [[nodiscard]] virtual Result<int> query_count(const Model::Td::User &user,
                                                  std::optional<int> machine_id,
                                                  std::int64_t timestamp_ms) = 0;
};

class TdProtocolClient : public ITdClient {
public:
    TdProtocolClient(Model::Td::Config config, ITdTransport &transport);

    [[nodiscard]] Result<CheckResponse> check(const Model::Td::User &user,
                                              int machine_id,
                                              std::int64_t timestamp_ms) override;
    [[nodiscard]] Result<TdRawResponse> upload_photo(int machine_id,
                                                     const ByteVector &photo,
                                                     std::int64_t timestamp_ms) override;
    [[nodiscard]] Result<int> query_count(const Model::Td::User &user,
                                          std::optional<int> machine_id,
                                          std::int64_t timestamp_ms) override;

private:
    [[nodiscard]] Result<TdRawResponse> request_json(const ByteVector &body, std::uint8_t request_type) const;

    Model::Td::Config m_config;
    ITdTransport &m_transport;
};

[[nodiscard]] Result<ByteVector> encode_frame(std::uint8_t request_type, const ByteVector &body);
[[nodiscard]] Result<FrameHeader> decode_frame_header(const ByteVector &header);
[[nodiscard]] Result<Frame> decode_frame(const ByteVector &frame);

[[nodiscard]] Result<nlohmann::json> build_check_data(const Model::Td::Config &config,
                                                      const Model::Td::User &user,
                                                      int machine_id,
                                                      std::int64_t timestamp_ms);
[[nodiscard]] Result<ByteVector> build_photo_payload(const Model::Td::Config &config,
                                                     int machine_id,
                                                     const ByteVector &photo,
                                                     std::int64_t timestamp_ms);

[[nodiscard]] std::string clean_server_message(std::string message);
[[nodiscard]] std::optional<int> extract_exercise_count(const std::string &message);
[[nodiscard]] Result<TdRawResponse> parse_raw_response(const nlohmann::json &json);
[[nodiscard]] Result<CheckResponse> parse_check_response(const nlohmann::json &json);

} // namespace UBAANext::Protocol::Td
