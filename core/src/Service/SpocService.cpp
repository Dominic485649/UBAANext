#include <UBAANext/Service/SpocService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/SpocParser.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <tuple>
#include <utility>

namespace UBAANext {

namespace {

constexpr const char *current_term_param = "YHrxtTavu6raCwC0/qdgYffB9evWHBkTng/XS4W6j3f/TPo02iEPSoegscDTRNzIPRG49o3RHl4JiFCXAiBkkA==";
constexpr const char *assignments_page_sql_id = "1713252980496efac7d5d9985e81693116d3e8a52ebf2b";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string resolve_spoc_url(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    auto value = Protocol::header_value(response, name);
    auto newline = value.find('\n');
    return newline == std::string::npos ? value : value.substr(0, newline);
}

std::string resolve_redirect_url(const std::string &base, const std::string &location) {
    return Protocol::resolve_location(base, location);
}

std::string extract_spoc_token(const std::string &url) {
    return Protocol::extract_query_parameter_anywhere(url, "token");
}

bool response_is_login(const HttpResponse &response) {
    return Protocol::is_session_expired_response(response, {}, false);
}

void apply_spoc_headers(HttpRequest &request) {
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Accept-Language"] = "zh-CN,zh;q=0.9";
    request.headers["User-Agent"] = kUserAgent;
    request.headers["X-Requested-With"] = "XMLHttpRequest";
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    if (json[key].is_number_float()) return std::to_string(json[key].get<double>());
    return {};
}

std::string escape_json(const std::string &value) {
    return nlohmann::json(value).dump();
}

std::string base64_encode(const std::string &data) {
    static constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -6;
    for (unsigned char c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

constexpr std::array<std::uint8_t, 256> sbox = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};

constexpr std::array<std::uint8_t, 11> rcon = {0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};

std::uint8_t xtime(std::uint8_t value) {
    return static_cast<std::uint8_t>((value << 1U) ^ ((value & 0x80U) ? 0x1bU : 0x00U));
}

std::array<std::uint8_t, 176> expand_key(const std::string &key) {
    std::array<std::uint8_t, 176> round_keys{};
    std::copy_n(reinterpret_cast<const std::uint8_t *>(key.data()), 16, round_keys.begin());
    int bytes_generated = 16;
    int rcon_iteration = 1;
    std::array<std::uint8_t, 4> temp{};
    while (bytes_generated < static_cast<int>(round_keys.size())) {
        for (int i = 0; i < 4; ++i) temp[i] = round_keys[bytes_generated - 4 + i];
        if (bytes_generated % 16 == 0) {
            std::rotate(temp.begin(), temp.begin() + 1, temp.end());
            for (auto &byte : temp) byte = sbox[byte];
            temp[0] ^= rcon[rcon_iteration++];
        }
        for (int i = 0; i < 4; ++i) {
            round_keys[bytes_generated] = round_keys[bytes_generated - 16] ^ temp[i];
            ++bytes_generated;
        }
    }
    return round_keys;
}

void add_round_key(std::array<std::uint8_t, 16> &state, const std::array<std::uint8_t, 176> &round_keys, int round) {
    for (int i = 0; i < 16; ++i) state[i] ^= round_keys[round * 16 + i];
}

void sub_bytes(std::array<std::uint8_t, 16> &state) {
    for (auto &byte : state) byte = sbox[byte];
}

void shift_rows(std::array<std::uint8_t, 16> &state) {
    auto tmp = state;
    state[1] = tmp[5]; state[5] = tmp[9]; state[9] = tmp[13]; state[13] = tmp[1];
    state[2] = tmp[10]; state[6] = tmp[14]; state[10] = tmp[2]; state[14] = tmp[6];
    state[3] = tmp[15]; state[7] = tmp[3]; state[11] = tmp[7]; state[15] = tmp[11];
}

void mix_columns(std::array<std::uint8_t, 16> &state) {
    for (int col = 0; col < 4; ++col) {
        auto *c = &state[col * 4];
        const auto a0 = c[0], a1 = c[1], a2 = c[2], a3 = c[3];
        const auto t = static_cast<std::uint8_t>(a0 ^ a1 ^ a2 ^ a3);
        c[0] ^= t ^ xtime(static_cast<std::uint8_t>(a0 ^ a1));
        c[1] ^= t ^ xtime(static_cast<std::uint8_t>(a1 ^ a2));
        c[2] ^= t ^ xtime(static_cast<std::uint8_t>(a2 ^ a3));
        c[3] ^= t ^ xtime(static_cast<std::uint8_t>(a3 ^ a0));
    }
}

std::array<std::uint8_t, 16> aes_encrypt_block(const std::array<std::uint8_t, 16> &block, const std::array<std::uint8_t, 176> &round_keys) {
    auto state = block;
    add_round_key(state, round_keys, 0);
    for (int round = 1; round < 10; ++round) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round_keys, round);
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, round_keys, 10);
    return state;
}

std::string aes_cbc_no_padding_encrypt(std::string plaintext, const std::string &key, const std::string &iv) {
    const auto remainder = plaintext.size() % 16;
    if (remainder != 0) plaintext.append(16 - remainder, '\0');
    auto round_keys = expand_key(key);
    std::array<std::uint8_t, 16> previous{};
    std::copy_n(reinterpret_cast<const std::uint8_t *>(iv.data()), 16, previous.begin());
    std::string encrypted;
    encrypted.reserve(plaintext.size());
    for (std::size_t offset = 0; offset < plaintext.size(); offset += 16) {
        std::array<std::uint8_t, 16> block{};
        for (std::size_t i = 0; i < 16; ++i) block[i] = static_cast<std::uint8_t>(static_cast<unsigned char>(plaintext[offset + i])) ^ previous[i];
        auto cipher = aes_encrypt_block(block, round_keys);
        for (auto byte : cipher) encrypted.push_back(static_cast<char>(byte));
        previous = cipher;
    }
    return encrypted;
}

std::string encrypt_spoc_param(const std::string &plain_text) {
    return base64_encode(aes_cbc_no_padding_encrypt(plain_text, "inco12345678ocni", "ocni12345678inco"));
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord summary_to_record(const Model::SpocAssignmentSummary &assignment) {
    return make_record(assignment.id, assignment.title, assignment.status, {
        {"courseId", assignment.course_id},
        {"courseName", assignment.course_name},
        {"teacher", assignment.teacher},
        {"startTime", assignment.start_time},
        {"dueTime", assignment.due_time},
        {"score", assignment.score},
        {"term", assignment.term_code},
        {"termName", assignment.term_name},
        {"submissionStatus", assignment.submission_status},
    });
}

Model::FeatureRecord detail_to_record(const Model::SpocAssignmentDetail &assignment) {
    return make_record(assignment.id, assignment.title, assignment.status, {
        {"courseId", assignment.course_id},
        {"startTime", assignment.start_time},
        {"dueTime", assignment.due_time},
        {"score", assignment.score},
        {"content", assignment.content},
        {"submissionStatus", assignment.submission_status},
        {"submittedAt", assignment.submitted_at},
    });
}

} // namespace

SpocService::SpocService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::string> SpocService::fetch_login_token() {
    (void)m_cache;
    std::string current_url = "https://spoc.buaa.edu.cn/spocnewht/cas";
    for (int i = 0; i < 8; ++i) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_spoc_url(current_url, m_mode);
        apply_spoc_headers(request);
        Protocol::disable_transport_redirects(request);
        auto response = m_http_client.send(request);
        if (!response) {
            if (m_mode == ConnectionMode::Direct && current_url == "https://spoc.buaa.edu.cn/spocnewht/cas" &&
                (response.error().code == ErrorCode::TlsError || response.error().message.find("TLS") != std::string::npos ||
                 response.error().message.find("handshake") != std::string::npos)) {
                current_url = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fspoc.buaa.edu.cn%2Fspocnewht%2FcasLogin";
                continue;
            }
            return make_error(ErrorCode::NetworkError, "获取 SPOC 登录 token 失败: " + Security::redact_sensitive_text(response.error().message));
        }

        auto current_token = extract_spoc_token(current_url);
        if (!current_token.empty()) return current_token;

        auto location = header_value(*response, "Location");
        auto location_token = extract_spoc_token(location);
        if (!location_token.empty()) return location_token;
        if (location.empty()) {
            if (response_is_login(*response)) {
                auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                             Protocol::DownstreamActivationStage::RedirectFollow,
                                                             Protocol::DownstreamSessionState::SsoRequired,
                                                             "SPOC 会话已过期，请重新登录",
                                                             Protocol::redact_url_query(current_url));
                return make_error(error.code, Protocol::to_error(error).message);
            }
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::ProtocolError,
                                                         "SPOC 登录跳转缺少 Location",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        current_url = resolve_redirect_url(current_url, location);
    }
    auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                 Protocol::DownstreamActivationStage::ArtifactExtract,
                                                 Protocol::DownstreamSessionState::TokenMissing,
                                                 "未能在 SPOC 登录跳转链中获取 token",
                                                 Protocol::redact_url_query(current_url));
    return make_error(error.code, Protocol::to_error(error).message);
}

Result<void> SpocService::perform_cas_login(const std::string &token) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_spoc_url("https://spoc.buaa.edu.cn/spocnewht/sys/casLogin", m_mode);
    apply_spoc_headers(request);
    request.headers["Content-Type"] = "application/json";
    request.headers["Token"] = "Inco-" + token;
    request.body = nlohmann::json{{"token", token}}.dump();

    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "SPOC CAS 登录失败: " + Security::redact_sensitive_text(response.error().message));
    if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "SPOC 会话已过期，请重新登录");
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "SPOC CAS 登录返回: " + std::to_string(response->status_code));

    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析 SPOC 登录 JSON 失败");
    auto content_result = unwrap_envelope(json, response->body);
    if (!content_result) return make_error(content_result.error().code, content_result.error().message);
    const auto &content = *content_result;
    auto role = json_string(content, "jsdm");
    if (role.empty() && content.contains("rolecode")) {
        if (content["rolecode"].is_string()) role = content["rolecode"].get<std::string>();
        else if (content["rolecode"].is_array() && !content["rolecode"].empty() && content["rolecode"][0].is_string()) role = content["rolecode"][0].get<std::string>();
    }
    if (role.empty() && content.contains("jsdmList") && content["jsdmList"].is_array() && !content["jsdmList"].empty() && content["jsdmList"][0].is_string()) {
        role = content["jsdmList"][0].get<std::string>();
    }
    if (role.empty()) return make_error(ErrorCode::ParseError, "SPOC 登录成功但未获取到角色信息");
    m_token = token;
    m_role_code = role;
    return {};
}

Result<void> SpocService::ensure_login(bool force_refresh) {
    if (!force_refresh && !m_token.empty() && !m_role_code.empty()) return {};
    auto token = fetch_login_token();
    if (!token) return make_error(token.error().code, token.error().message);
    return perform_cas_login(*token);
}

Result<nlohmann::json> SpocService::unwrap_envelope(const nlohmann::json &envelope, const std::string &body) {
    auto code = envelope.value("code", 0);
    if (code == 200) {
        if (envelope.contains("content")) return envelope["content"];
        return nlohmann::json::object();
    }
    auto message = envelope.value("msg", envelope.value("msg_en", std::string("SPOC 请求失败")));
    auto text = message + " " + body;
    if (text.find("登录") != std::string::npos || text.find("token") != std::string::npos || text.find("未认证") != std::string::npos || text.find("未登录") != std::string::npos || text.find("权限") != std::string::npos) {
        return make_error(ErrorCode::SessionExpired, "SPOC 会话已过期，请重新登录");
    }
    return make_error(ErrorCode::NetworkError, Security::redact_sensitive_text(message));
}

Result<nlohmann::json> SpocService::get_envelope_once(const std::string &url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode(url, m_mode);
    apply_spoc_headers(request);
    request.headers["Token"] = "Inco-" + m_token;
    request.headers["RoleCode"] = m_role_code;
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "请求 SPOC 失败: " + Security::redact_sensitive_text(response.error().message));
    if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "SPOC 会话已过期，请重新登录");
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "SPOC 请求返回: " + std::to_string(response->status_code));
    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析 SPOC JSON 失败");
    return unwrap_envelope(json, response->body);
}

Result<nlohmann::json> SpocService::post_envelope_once(const std::string &url, const nlohmann::json &body) {
    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode(url, m_mode);
    apply_spoc_headers(request);
    request.headers["Content-Type"] = "application/json";
    request.headers["Token"] = "Inco-" + m_token;
    request.headers["RoleCode"] = m_role_code;
    request.body = body.dump();
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "请求 SPOC 失败: " + Security::redact_sensitive_text(response.error().message));
    if (response_is_login(*response)) return make_error(ErrorCode::SessionExpired, "SPOC 会话已过期，请重新登录");
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "SPOC 请求返回: " + std::to_string(response->status_code));
    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析 SPOC JSON 失败");
    return unwrap_envelope(json, response->body);
}

Result<nlohmann::json> SpocService::get_envelope(const std::string &url) {
    auto result = get_envelope_once(url);
    if (!result && result.error().code == ErrorCode::SessionExpired) {
        m_token.clear();
        m_role_code.clear();
        auto refreshed = ensure_login(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        return get_envelope_once(url);
    }
    return result;
}

Result<nlohmann::json> SpocService::post_envelope(const std::string &url, const nlohmann::json &body) {
    auto result = post_envelope_once(url, body);
    if (!result && result.error().code == ErrorCode::SessionExpired) {
        m_token.clear();
        m_role_code.clear();
        auto refreshed = ensure_login(true);
        if (!refreshed) return make_error(refreshed.error().code, refreshed.error().message);
        return post_envelope_once(url, body);
    }
    return result;
}

Result<std::vector<Model::SpocAssignmentSummary>> SpocService::list_assignment_summaries_once() {
    auto term = post_envelope("https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryOne", nlohmann::json{{"param", current_term_param}});
    if (!term) return make_error(term.error().code, term.error().message);
    auto term_code = json_string(*term, "mrxq");
    auto term_name = json_string(*term, "dqxq");
    if (term_code.empty()) return make_error(ErrorCode::ParseError, "SPOC 当前学期缺少 mrxq 字段");

    std::map<std::string, std::pair<std::string, std::string>> courses;
    auto course_list = get_envelope("https://spoc.buaa.edu.cn/spocnewht/jxkj/queryKclb?kcmc=&xnxq=" + term_code);
    if (course_list && course_list->is_array()) {
        for (const auto &course : *course_list) {
            courses[json_string(course, "kcid")] = {json_string(course, "kcmc"), json_string(course, "skjs")};
        }
    }

    std::vector<Model::SpocAssignmentSummary> records;
    int page_num = 1;
    while (page_num <= 50) {
        auto plain = "{\"pageSize\":15,\"pageNum\":" + std::to_string(page_num) + ",\"sqlid\":" + escape_json(assignments_page_sql_id) + ",\"xnxq\":" + escape_json(term_code) + ",\"kcid\":\"\",\"yzwz\":\"\"}";
        auto page = post_envelope("https://spoc.buaa.edu.cn/spocnewht/inco/ht/queryListByPage", nlohmann::json{{"param", encrypt_spoc_param(plain)}});
        if (!page) return make_error(page.error().code, page.error().message);
        auto page_records = Parser::parse_spoc_assignments_page(*page, courses, term_code, term_name);
        records.insert(records.end(), page_records.begin(), page_records.end());
        auto list = page->contains("list") && (*page)["list"].is_array() ? (*page)["list"] : nlohmann::json::array();
        bool has_next = page->value("hasNextPage", false);
        int pages = page->value("pages", page_num);
        if (!has_next || page_num >= pages || list.empty()) break;
        ++page_num;
    }
    return records;
}

Result<std::vector<Model::SpocAssignmentSummary>> SpocService::list_assignment_summaries() {
    return list_assignment_summaries({});
}

Result<std::vector<Model::SpocAssignmentSummary>> SpocService::list_assignment_summaries(const SpocAssignmentQuery &query) {
    auto login = ensure_login();
    if (!login) return make_error(login.error().code, login.error().message);
    auto result = list_assignment_summaries_once();
    if (!result) return result;

    std::vector<Model::SpocAssignmentSummary> records;
    for (auto record : *result) {
        if (query.pending_only && record.status == "submitted") continue;
        if (!query.include_expired && record.status == "expired") continue;
        records.push_back(std::move(record));
    }
    std::sort(records.begin(), records.end(), [](const auto &lhs, const auto &rhs) {
        auto lhs_due = lhs.due_time.empty() ? "9999-99-99 99:99:99" : lhs.due_time;
        auto rhs_due = rhs.due_time.empty() ? "9999-99-99 99:99:99" : rhs.due_time;
        return std::tie(lhs_due, lhs.course_name, lhs.title) < std::tie(rhs_due, rhs.course_name, rhs.title);
    });
    return records;
}

Result<std::vector<Model::FeatureRecord>> SpocService::list_assignments() {
    return list_assignments({});
}

Result<std::vector<Model::FeatureRecord>> SpocService::list_assignments(const SpocAssignmentQuery &query) {
    auto summaries = list_assignment_summaries(query);
    if (!summaries) return make_error(summaries.error().code, summaries.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &summary : *summaries) records.push_back(summary_to_record(summary));
    return records;
}

Result<Model::SpocAssignmentDetail> SpocService::assignment_detail_once(const std::string &assignment_id) {
    auto detail = get_envelope("https://spoc.buaa.edu.cn/spocnewht/kczy/queryKczyInfoByid?id=" + assignment_id);
    if (!detail) return make_error(detail.error().code, detail.error().message);
    auto submission = get_envelope("https://spoc.buaa.edu.cn/spocnewht/kczy/queryXsSubmitKczyInfo?kczyid=" + assignment_id);
    return Parser::parse_spoc_assignment_detail(*detail, submission ? &*submission : nullptr, assignment_id);
}

Result<Model::SpocAssignmentDetail> SpocService::assignment_detail(const std::string &assignment_id) {
    if (assignment_id.empty()) return make_error(ErrorCode::InvalidArgument, "spoc assignment show 需要 --id <assignmentId>");
    auto login = ensure_login();
    if (!login) return make_error(login.error().code, login.error().message);
    auto result = assignment_detail_once(assignment_id);
    return result;
}

Result<Model::FeatureRecord> SpocService::show_assignment(const std::string &assignment_id) {
    auto detail = assignment_detail(assignment_id);
    if (!detail) return make_error(detail.error().code, detail.error().message);
    return detail_to_record(*detail);
}

} // namespace UBAANext
