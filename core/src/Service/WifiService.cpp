#include <UBAANext/Service/WifiService.hpp>

#include <UBAANext/Crypto/ProtocolCrypto.hpp>
#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace UBAANext {
namespace {

std::string url_encode(const std::string &value) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out << static_cast<char>(ch);
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

std::string now_millis_text() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

std::string parse_between(const std::string &text, const std::string &begin, const std::string &end) {
    auto start = text.find(begin);
    if (start == std::string::npos) return {};
    start += begin.size();
    auto finish = text.find(end, start);
    if (finish == std::string::npos) return {};
    return text.substr(start, finish - start);
}

std::string parse_ac_id(const std::string &body) {
    auto ac = parse_between(body, "ac_id=", "&");
    if (!ac.empty()) return ac;
    ac = parse_between(body, "ac_id=", "\"");
    if (!ac.empty()) return ac;
    return parse_between(body, "ac_id=", "'");
}

std::vector<unsigned char> bytes_of(const std::string &value) {
    return {value.begin(), value.end()};
}

std::string query(std::initializer_list<std::pair<std::string, std::string>> params) {
    std::string out;
    bool first = true;
    for (const auto &[key, value] : params) {
        if (!first) out.push_back('&');
        first = false;
        out += url_encode(key);
        out.push_back('=');
        out += url_encode(value);
    }
    return out;
}

Model::FeatureRecord wifi_record(const Model::WifiResult &wifi) {
    Model::FeatureRecord record;
    record.id = wifi.action;
    record.title = "BUAA WiFi";
    record.status = wifi.status;
    record.fields["action"] = wifi.action;
    record.fields["username"] = wifi.username;
    record.fields["ip"] = wifi.ip;
    record.fields["acId"] = wifi.ac_id;
    record.fields["message"] = wifi.message;
    return record;
}

} // namespace

WifiService::WifiService(IHttpClient &http_client,
                         INetworkEnvironment *network_environment,
                         ICryptoProvider &crypto_provider,
                         Model::WifiCredentials credentials)
    : m_http_client(http_client),
      m_network_environment(network_environment),
      m_crypto_provider(crypto_provider),
      m_credentials(std::move(credentials)) {}

void WifiService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> WifiService::login() {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto result = execute("login");
    if (!result) return make_error(result.error().code, result.error().message);
    Model::MutationResult mutation;
    mutation.accepted = true;
    mutation.message = result->message;
    mutation.summary = wifi_record(*result);
    return mutation;
}

Result<Model::MutationResult> WifiService::logout() {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto result = execute("logout");
    if (!result) return make_error(result.error().code, result.error().message);
    Model::MutationResult mutation;
    mutation.accepted = true;
    mutation.message = result->message;
    mutation.summary = wifi_record(*result);
    return mutation;
}

Result<Model::WifiResult> WifiService::execute(const std::string &action) {
    if (m_credentials.username.empty()) return make_error(ErrorCode::InvalidArgument, "wifi " + action + " 需要账号；请提供 --username 或先 login 保存账号");
    if (action == "login" && m_credentials.password.empty()) return make_error(ErrorCode::InvalidArgument, "wifi login 需要密码；请提供 --password 或先 login 保存密码");
    if (m_network_environment == nullptr) return make_error(ErrorCode::UnsupportedPlatform, "当前平台未提供 WiFi 网络环境探测");

    auto campus = m_network_environment->is_on_campus_network();
    if (!campus) return make_error(campus.error().code, campus.error().message);
    if (!*campus) return make_error(ErrorCode::NetworkError, "当前未连接 BUAA-WiFi 或 BUAA-Mobile");

    auto ip = m_network_environment->local_ipv4();
    if (!ip) return make_error(ip.error().code, ip.error().message);
    if (ip->empty()) return make_error(ErrorCode::NetworkError, "未获取到本机 IPv4");

    HttpRequest gateway;
    gateway.method = HttpMethod::Get;
    gateway.url = "http://gw.buaa.edu.cn";
    gateway.headers["User-Agent"] = kUserAgent;
    auto gateway_response = m_http_client.send(gateway);
    if (!gateway_response) return make_error(ErrorCode::NetworkError, "请求 WiFi 网关失败: " + Security::redact_sensitive_text(gateway_response.error().message));
    if (gateway_response->status_code < 200 || gateway_response->status_code >= 400) return make_error(ErrorCode::NetworkError, "WiFi 网关返回: " + std::to_string(gateway_response->status_code));
    const auto ac_id = parse_ac_id(gateway_response->body);
    if (ac_id.empty()) return make_error(ErrorCode::ParseError, "WiFi 网关响应缺少 ac_id");

    const auto time = now_millis_text();
    std::string portal_query;
    if (action == "login") {
        HttpRequest challenge;
        challenge.method = HttpMethod::Get;
        challenge.url = "https://gw.buaa.edu.cn/cgi-bin/get_challenge?" + query({{"callback", time}, {"username", m_credentials.username}, {"ip", *ip}, {"_", time}});
        challenge.headers["User-Agent"] = kUserAgent;
        auto challenge_response = m_http_client.send(challenge);
        if (!challenge_response) return make_error(ErrorCode::NetworkError, "请求 WiFi challenge 失败: " + Security::redact_sensitive_text(challenge_response.error().message));
        if (challenge_response->status_code < 200 || challenge_response->status_code >= 400) return make_error(ErrorCode::NetworkError, "WiFi challenge 返回: " + std::to_string(challenge_response->status_code));
        const auto token = parse_between(challenge_response->body, "\"challenge\":\"", "\"");
        if (token.empty()) return make_error(ErrorCode::ParseError, "WiFi challenge 响应缺少 challenge 字段");
        const auto token_bytes = bytes_of(token);
        const auto info_json = nlohmann::json{{"username", m_credentials.username},
                                              {"password", m_credentials.password},
                                              {"ip", *ip},
                                              {"acid", ac_id},
                                              {"enc_ver", "srun_bx1"}}.dump();
        const auto info = Crypto::srun_xencode(bytes_of(info_json), token_bytes);
        const auto password_md5 = Crypto::bytes_to_hex(Crypto::hmac_md5_digest(token_bytes, bytes_of(m_credentials.password)));
        const auto sum_plain = token + m_credentials.username + token + password_md5 + token + ac_id + token + *ip + token + "200" + token + "1" + token + info;
        auto sha1 = m_crypto_provider.sha1_digest(bytes_of(sum_plain));
        if (!sha1) return make_error(sha1.error().code, sha1.error().message);
        const auto checksum = Crypto::bytes_to_hex(*sha1);
        portal_query = query({{"callback", time},
                              {"action", "login"},
                              {"username", m_credentials.username},
                              {"password", "{MD5}" + password_md5},
                              {"ac_id", ac_id},
                              {"ip", *ip},
                              {"chksum", checksum},
                              {"info", info},
                              {"n", "200"},
                              {"type", "1"},
                              {"os", "Windows+10"},
                              {"name", "Windows"},
                              {"double_stack", "0"},
                              {"_", time}});
    } else if (action == "logout") {
        portal_query = query({{"callback", time},
                              {"action", "logout"},
                              {"username", m_credentials.username},
                              {"ac_id", ac_id},
                              {"ip", *ip}});
    } else {
        return make_error(ErrorCode::InvalidArgument, "未知 WiFi 操作: " + action);
    }

    HttpRequest portal;
    portal.method = HttpMethod::Get;
    portal.url = "https://gw.buaa.edu.cn/cgi-bin/srun_portal?" + portal_query;
    portal.headers["User-Agent"] = kUserAgent;
    auto portal_response = m_http_client.send(portal);
    if (!portal_response) return make_error(ErrorCode::NetworkError, "请求 WiFi portal 失败: " + Security::redact_sensitive_text(portal_response.error().message));
    if (portal_response->status_code < 200 || portal_response->status_code >= 400) return make_error(ErrorCode::NetworkError, "WiFi portal 返回: " + std::to_string(portal_response->status_code));
    if (portal_response->body.find("\"error\":\"ok\"") == std::string::npos) {
        return make_error(ErrorCode::NetworkError, "WiFi " + action + " 失败: " + Security::redact_sensitive_text(portal_response->body));
    }

    Model::WifiResult result;
    result.action = action;
    result.username = m_credentials.username;
    result.ip = *ip;
    result.ac_id = ac_id;
    result.status = action == "login" ? "logged-in" : "logged-out";
    result.message = action == "login" ? "WiFi 登录成功" : "WiFi 登出成功";
    return result;
}

} // namespace UBAANext
