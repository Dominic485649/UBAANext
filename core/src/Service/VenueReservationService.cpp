#include <UBAANext/Service/VenueReservationService.hpp>

#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/VenueReservationParser.hpp>
#include <UBAANext/Protocol/AppBuaaSession.hpp>
#include <UBAANext/Protocol/AuthorizedDownstreamRequestExecutor.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace UBAANext {

namespace {

constexpr const char *base_url = "https://cgyy.buaa.edu.cn/venue-zhjs-server";
constexpr const char *referer_url = "https://cgyy.buaa.edu.cn/venue-zhjs/mobileReservation";
constexpr const char *app_key = "8fceb735082b5a529312040b58ea780b";
constexpr const char *sign_prefix = "c640ca392cd45fb3a55b00a63a86c618";

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string url_encode(const std::string &value) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.' || ch == '~') out << static_cast<char>(ch);
        else out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
    return out.str();
}

std::string form_encode(const std::map<std::string, std::string> &form) {
    std::string body;
    for (const auto &[key, value] : form) {
        if (!body.empty()) body += '&';
        body += url_encode(key) + "=" + url_encode(value);
    }
    return body;
}

std::int64_t now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

Result<std::string> md5_hex(ICryptoProvider &crypto, const std::string &input) {
    return crypto.md5_hex(input);
}

Result<long long> parse_numeric_id(const std::string &value, const char *name) {
    long long parsed = 0;
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed <= 0) {
        return make_error(ErrorCode::InvalidArgument, std::string("cgyy 需要数字形式的 ") + name);
    }
    return parsed;
}

int count_joiners(const std::string &joiners) {
    int count = 0;
    bool in_entry = false;
    for (char ch : joiners) {
        if (ch == '\n' || ch == '\r' || ch == ',' || ch == ';') {
            if (in_entry) ++count;
            in_entry = false;
        } else if (!std::isspace(static_cast<unsigned char>(ch))) {
            in_entry = true;
        }
    }
    if (in_entry) ++count;
    return count < 1 ? 1 : count;
}

Result<void> require_direct_mode(ConnectionMode mode) {
    if (mode == ConnectionMode::Direct) return {};
    auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                 Protocol::DownstreamActivationStage::Probe,
                                                 Protocol::DownstreamSessionState::UnsupportedMode,
                                                 "CGYY 真实功能需要直连模式，请使用 --mode direct");
    return make_error(error.code, Protocol::to_error(error).message);
}

Result<std::string> sign_request(ICryptoProvider &crypto, const std::string &path, const std::map<std::string, std::string> &params, std::int64_t timestamp) {
    std::string normalized = path.empty() || path.front() != '/' ? "/" + path : path;
    std::string payload = sign_prefix + normalized;
    for (const auto &[key, value] : params) {
        if (value.empty()) continue;
        if (key == "gmtCreate" || key == "gmtModified" || key == "creator" || key == "modifier" || key == "id" || key == "_index" || key == "_rowKey") continue;
        payload += key + value;
    }
    payload += std::to_string(timestamp);
    payload += " ";
    payload += sign_prefix;
    return md5_hex(crypto, payload);
}

bool response_is_login(const HttpResponse &response) {
    return response.status_code == 401 || response.status_code == 403 ||
           response.body.find("name=\"execution\"") != std::string::npos ||
           response.body.find("统一身份认证") != std::string::npos ||
           response.body.find("username_password") != std::string::npos;
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    return Protocol::header_value(response, name);
}

std::string resolve_redirect_url(const std::string &base, const std::string &location) {
    return Protocol::resolve_location(base, location);
}

bool is_cgyy_sso_cookie_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (name == "sso_buaa_zhjs_token") return true;
    return name.find("sso") != std::string::npos &&
           name.find("token") != std::string::npos &&
           (name.find("zhjs") != std::string::npos || name.find("cgyy") != std::string::npos || name.find("venue") != std::string::npos);
}

std::string extract_sso_token_from_set_cookie(const std::string &set_cookie) {
    size_t pos = 0;
    while (pos <= set_cookie.size()) {
        auto newline = set_cookie.find('\n', pos);
        auto line = set_cookie.substr(pos, newline == std::string::npos ? std::string::npos : newline - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto semi = line.find(';');
        auto pair = line.substr(0, semi);
        auto eq = pair.find('=');
        if (eq != std::string::npos && is_cgyy_sso_cookie_name(pair.substr(0, eq))) {
            auto value = pair.substr(eq + 1);
            if (!value.empty()) return value;
        }
        if (newline == std::string::npos) break;
        pos = newline + 1;
    }
    return {};
}

std::string sso_token_from_cookie_jar(ICookieStore *cookie_store) {
    if (!cookie_store) return {};
    auto *cookies = cookie_store->current();
    if (!cookies) return {};
    if (auto token = cookies->get_cookie("cgyy.buaa.edu.cn", "sso_buaa_zhjs_token")) {
        return *token;
    }
    return {};
}

Result<std::string> acquire_sso_token(IHttpClient &http_client, ICookieStore *cookie_store, ConnectionMode mode) {
    std::string current_url = "https://cgyy.buaa.edu.cn/venue-zhjs-server/sso/manageLogin";
    for (int redirects = 0; redirects < 8; ++redirects) {
        HttpRequest request;
        request.method = HttpMethod::Get;
        request.url = resolve_for_mode(current_url, mode);
        request.headers["Accept"] = "application/json, text/plain, */*";
        request.headers["Referer"] = resolve_for_mode(referer_url, mode);
        Protocol::disable_transport_redirects(request);
        request.redirect.post_policy = RedirectPostPolicy::PreserveMethod;
        request.redirect.restrict_to_http_https = true;

        auto response = http_client.send(request);
        if (!response) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::Unavailable,
                                                         "激活研讨室 SSO 失败: " + response.error().message,
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        if (response_is_login(*response)) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::SsoRequired,
                                                         "研讨室会话已过期，请重新登录",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }

        auto token = extract_sso_token_from_set_cookie(header_value(*response, "Set-Cookie"));
        if (token.empty()) token = sso_token_from_cookie_jar(cookie_store);
        if (!token.empty()) return token;
        if (response->status_code < 300 || response->status_code >= 400) return std::string{};
        auto location = header_value(*response, "Location");
        if (location.empty()) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::ProtocolError,
                                                         "研讨室跳转缺少 Location",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        current_url = resolve_redirect_url(current_url, location);
    }
    auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                 Protocol::DownstreamActivationStage::RedirectFollow,
                                                 Protocol::DownstreamSessionState::Unavailable,
                                                 "研讨室跳转次数过多",
                                                 Protocol::redact_url_query(current_url));
    return make_error(error.code, Protocol::to_error(error).message);
}

std::string json_string(const nlohmann::json &json, const char *key) {
    if (!json.contains(key) || json[key].is_null()) return {};
    if (json[key].is_string()) return json[key].get<std::string>();
    if (json[key].is_number_integer()) return std::to_string(json[key].get<long long>());
    return {};
}

Model::FeatureRecord make_record(std::string id, std::string title, std::string status, std::map<std::string, std::string> fields = {}) {
    Model::FeatureRecord record;
    record.id = std::move(id);
    record.title = std::move(title);
    record.status = std::move(status);
    record.fields = std::move(fields);
    return record;
}

Model::FeatureRecord to_record(const Model::VenueSite &site) {
    return make_record(site.id, site.name, "available", {{"venueId", site.venue_id}, {"venueName", site.venue_name}, {"campusName", site.campus_name}});
}

Model::FeatureRecord to_record(const Model::VenuePurposeType &purpose) {
    return make_record(purpose.id, purpose.name, "available");
}

Model::FeatureRecord to_record(const Model::VenueSpaceInfo &space) {
    std::map<std::string, std::string> fields{{"date", space.date}, {"siteId", space.site_id}, {"token", space.token}};
    if (!space.time_id.empty()) fields["timeId"] = space.time_id;
    if (!space.time_label.empty()) fields["time"] = space.time_label;
    if (!space.status.empty()) fields["reservationStatus"] = space.status;
    if (!space.reservable.empty()) fields["reservable"] = space.reservable;
    auto status = space.reservable.empty() || space.reservable == "true" ? "available" : "unavailable";
    return make_record(space.id, space.name, status, std::move(fields));
}

Model::FeatureRecord to_record(const Model::VenueOrder &order) {
    return make_record(order.id, order.title, order.status, {{"reservationDate", order.reservation_date}, {"space", order.space}, {"site", order.site}, {"phone", order.phone}, {"joiners", order.joiners}});
}

template <typename T>
std::vector<Model::FeatureRecord> to_records(const std::vector<T> &items) {
    std::vector<Model::FeatureRecord> records;
    records.reserve(items.size());
    for (const auto &item : items) records.push_back(to_record(item));
    return records;
}

} // namespace

VenueReservationService::VenueReservationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : VenueReservationService(http_client, nullptr, cache, mode, default_crypto_provider()) {}

VenueReservationService::VenueReservationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto)
    : VenueReservationService(http_client, nullptr, cache, mode, crypto) {}

VenueReservationService::VenueReservationService(IHttpClient &http_client, ICookieStore *cookie_store, ICacheStore &cache, ConnectionMode mode, ICryptoProvider &crypto)
    : m_http_client(http_client), m_cookie_store(cookie_store), m_cache(cache), m_mode(mode), m_crypto(crypto) {}

Result<void> VenueReservationService::ensure_login(bool force_refresh) {
    auto direct_mode = require_direct_mode(m_mode);
    if (!direct_mode) return make_error(direct_mode.error().code, direct_mode.error().message);
    (void)m_cache;
    if (!force_refresh && !m_access_token.empty()) return {};

    auto sso_token = acquire_sso_token(m_http_client, m_cookie_store, m_mode);
    if (!sso_token) return make_error(sso_token.error().code, sso_token.error().message);
    if (sso_token->empty()) {
        auto sync = Protocol::AppBuaa::ensure_session(m_http_client,
                                                      m_mode,
                                                      "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fcgyy.buaa.edu.cn%2Fvenue-zhjs-server%2Fsso%2FmanageLogin",
                                                      kUserAgent);
        if (!sync) return make_error(sync.error().code, sync.error().message);
        sso_token = acquire_sso_token(m_http_client, m_cookie_store, m_mode);
        if (!sso_token) return make_error(sso_token.error().code, sso_token.error().message);
    }
    if (sso_token->empty()) {
        auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                     Protocol::DownstreamActivationStage::ArtifactExtract,
                                                     Protocol::DownstreamSessionState::TokenMissing,
                                                     "未获取到研讨室 SSO Token");
        return make_error(error.code, Protocol::to_error(error).message);
    }

    auto login = request_json(HttpMethod::Post, "/api/login", {}, {}, {{"Sso-Token", *sso_token}}, false, false);
    if (!login) return make_error(login.error().code, login.error().message);
    if (login->contains("token") && (*login)["token"].is_object()) {
        m_access_token = json_string((*login)["token"], "access_token");
    }
    if (m_access_token.empty()) {
        auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Cgyy,
                                                     Protocol::DownstreamActivationStage::TokenExchange,
                                                     Protocol::DownstreamSessionState::TokenMissing,
                                                     "研讨室登录成功但未返回 access_token");
        return make_error(error.code, Protocol::to_error(error).message);
    }
    return {};
}

Result<nlohmann::json> VenueReservationService::request_json(HttpMethod method,
                                                             const std::string &path,
                                                             const std::map<std::string, std::string> &params,
                                                             const std::map<std::string, std::string> &form,
                                                             const std::map<std::string, std::string> &extra_headers,
                                                             bool authorize,
                                                             bool allow_retry) {
    if (authorize) {
        auto login = ensure_login();
        if (!login) return make_error(login.error().code, login.error().message);
    }

    auto timestamp = now_millis();
    auto sign_params = method == HttpMethod::Get ? params : form;
    auto request_params = params;
    if (method == HttpMethod::Get && request_params.find("nocache") == request_params.end()) request_params["nocache"] = std::to_string(timestamp);
    auto sign = sign_request(m_crypto, path, method == HttpMethod::Get ? request_params : sign_params, timestamp);
    if (!sign) return make_error(sign.error().code, sign.error().message);

    HttpRequest request;
    request.method = method;
    request.url = resolve_for_mode(std::string(base_url) + path, m_mode);
    if (method == HttpMethod::Get && !request_params.empty()) {
        request.url += "?" + form_encode(request_params);
    }
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Referer"] = referer_url;
    request.headers["app-key"] = app_key;
    request.headers["timestamp"] = std::to_string(timestamp);
    request.headers["sign"] = *sign;
    for (const auto &[key, value] : extra_headers) request.headers[key] = value;
    if (method != HttpMethod::Get) {
        request.headers["Content-Type"] = "application/x-www-form-urlencoded";
        request.body = form_encode(form);
    }

    Protocol::AuthorizedRequestHooks hooks;
    hooks.system = Protocol::DownstreamSystemId::Cgyy;
    hooks.expired_message = "研讨室会话已过期，请重新登录";
    hooks.ensure_authorized = [this](bool force_refresh) { return ensure_login(force_refresh); };
    hooks.invalidate = [this]() { m_access_token.clear(); };
    hooks.decorate_request = [this](HttpRequest &authorized_request) {
        authorized_request.headers["cgAuthorization"] = m_access_token;
    };
    hooks.is_expired_response = [](const HttpResponse &response) {
        return response_is_login(response);
    };

    auto response = Protocol::send_authorized_request(m_http_client, std::move(request), std::move(hooks), authorize, allow_retry);
    if (!response) return make_error(response.error().code, response.error().message);
    if (response->status_code != 200) return make_error(ErrorCode::NetworkError, "研讨室请求返回: " + std::to_string(response->status_code));
    auto json = nlohmann::json::parse(response->body, nullptr, false);
    if (json.is_discarded()) return make_error(ErrorCode::ParseError, "解析研讨室 JSON 失败");
    if (json.value("code", 200) != 200) return make_error(ErrorCode::NetworkError, Security::redact_sensitive_text(json.value("message", std::string("研讨室请求失败"))));
    if (json.contains("data")) return json["data"];
    return json;
}

Result<std::vector<Model::VenueSite>> VenueReservationService::venue_sites() {
    auto direct_mode = require_direct_mode(m_mode);
    if (!direct_mode) return make_error(direct_mode.error().code, direct_mode.error().message);
    auto data = request_json(HttpMethod::Get, "/api/front/website/venues", {{"page", "-1"}, {"size", "-1"}, {"reservationRoleId", "3"}});
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_venue_sites(*data);
}

Result<std::vector<Model::VenuePurposeType>> VenueReservationService::purpose_types() {
    auto direct_mode = require_direct_mode(m_mode);
    if (!direct_mode) return make_error(direct_mode.error().code, direct_mode.error().message);
    auto data = request_json(HttpMethod::Get, "/api/codes");
    std::vector<Model::VenuePurposeType> records;
    if (data) records = Parser::parse_venue_purpose_types(*data);
    if (!records.empty()) return records;
    return std::vector<Model::VenuePurposeType>{{"1", "导学活动类"}, {"2", "学业支持类"}, {"3", "学术研讨类"}, {"4", "党建活动类"}, {"5", "工作会议类"}, {"6", "团队建设类"}, {"7", "培训面试类"}, {"8", "博雅课程类"}, {"9", "讲座、沙龙研讨类"}, {"10", "其他特色活动类"}};
}

Result<std::vector<Model::VenueSpaceInfo>> VenueReservationService::day_spaces(const std::string &date, const std::string &site_id) {
    if (date.empty() || site_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy day-info 需要 --date 和 --id/--site-id");
    auto parsed_site_id = parse_numeric_id(site_id, "--site-id <id>");
    if (!parsed_site_id) return make_error(parsed_site_id.error().code, parsed_site_id.error().message);
    auto data = request_json(HttpMethod::Get, "/api/reservation/day/info", {{"searchDate", date}, {"venueSiteId", site_id}});
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_venue_day_info(*data, site_id);
}

Result<std::vector<Model::VenueOrder>> VenueReservationService::orders(int page, int size) {
    auto upstream_page = page < 1 ? 0 : page - 1;
    auto data = request_json(HttpMethod::Get, "/api/orders/mine", {{"page", std::to_string(upstream_page)}, {"size", std::to_string(size < 1 ? 20 : size)}});
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_venue_orders(*data);
}

Result<Model::VenueOrder> VenueReservationService::order_detail(const std::string &order_id) {
    if (order_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy order show 需要 --order-id");
    auto parsed_order_id = parse_numeric_id(order_id, "--order-id <id>");
    if (!parsed_order_id) return make_error(parsed_order_id.error().code, parsed_order_id.error().message);
    auto data = request_json(HttpMethod::Get, "/api/orders/" + order_id);
    if (!data) return make_error(data.error().code, data.error().message);
    return Parser::parse_venue_order_detail(*data, order_id);
}

Result<std::vector<Model::FeatureRecord>> VenueReservationService::list_sites() {
    auto records = venue_sites();
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<std::vector<Model::FeatureRecord>> VenueReservationService::list_purpose_types() {
    auto records = purpose_types();
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<std::vector<Model::FeatureRecord>> VenueReservationService::day_info(const std::string &date, const std::string &site_id) {
    auto records = day_spaces(date, site_id);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<std::vector<Model::FeatureRecord>> VenueReservationService::list_orders(int page, int size) {
    auto records = orders(page, size);
    if (!records) return make_error(records.error().code, records.error().message);
    return to_records(*records);
}

Result<Model::FeatureRecord> VenueReservationService::show_order(const std::string &order_id) {
    auto record = order_detail(order_id);
    if (!record) return make_error(record.error().code, record.error().message);
    return to_record(*record);
}

Result<Model::FeatureRecord> VenueReservationService::lock_code() {
    auto data = request_json(HttpMethod::Get, "/api/orders/lock/code");
    if (!data) return make_error(data.error().code, data.error().message);
    return make_record("lock-code", "研讨室门锁码", "available", {{"raw", data->dump()}});
}

void VenueReservationService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> VenueReservationService::reserve(const std::string &site_id,
                                                                  const std::string &space_id,
                                                                  const std::string &date,
                                                                  const std::string &time_id,
                                                                  const std::string &purpose_type,
                                                                  const std::string &theme,
                                                                  const std::string &phone,
                                                                  const std::string &joiners,
                                                                  const std::string &captcha,
                                                                  const std::string &token) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (site_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --site-id <id>");
    if (space_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --space-id <id>");
    if (date.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --date <yyyy-MM-dd>");
    if (time_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --id <time-id>");
    if (purpose_type.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --purpose-type <id>");
    if (theme.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --theme");
    if (phone.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --phone");
    if (joiners.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --joiners");
    if (captcha.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要用户提供 --captcha，不会自动绕过验证码");
    if (token.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy reserve 需要 --token，请先运行 day-info 获取预约上下文");

    auto parsed_space_id = parse_numeric_id(space_id, "--space-id <id>");
    if (!parsed_space_id) return make_error(parsed_space_id.error().code, parsed_space_id.error().message);
    auto parsed_time_id = parse_numeric_id(time_id, "--id <time-id>");
    if (!parsed_time_id) return make_error(parsed_time_id.error().code, parsed_time_id.error().message);
    auto parsed_site_id = parse_numeric_id(site_id, "--site-id <id>");
    if (!parsed_site_id) return make_error(parsed_site_id.error().code, parsed_site_id.error().message);
    auto parsed_purpose_type = parse_numeric_id(purpose_type, "--purpose-type <id>");
    if (!parsed_purpose_type) return make_error(parsed_purpose_type.error().code, parsed_purpose_type.error().message);

    auto selection = nlohmann::json::array({nlohmann::json{{"spaceId", *parsed_space_id}, {"timeId", *parsed_time_id}}}).dump();
    auto created = request_json(HttpMethod::Post,
                                "/api/reservation/order/info",
                                {},
                                {{"venueSiteId", site_id}, {"reservationDate", date}, {"weekStartDate", date}, {"reservationOrderJson", selection}, {"token", token}});
    if (!created) return make_error(created.error().code, created.error().message);

    auto data = request_json(HttpMethod::Post,
                             "/api/reservation/order/submit",
                             {},
                             {{"venueSiteId", site_id},
                              {"reservationDate", date},
                              {"reservationOrderJson", selection},
                              {"weekStartDate", date},
                              {"phone", phone},
                              {"theme", theme},
                              {"purposeType", purpose_type},
                              {"joinerNum", std::to_string(count_joiners(joiners))},
                              {"activityContent", theme},
                              {"joiners", joiners},
                              {"isPhilosophySocialSciences", "0"},
                              {"isOffSchoolJoiner", "0"},
                              {"captchaVerification", captcha},
                              {"token", token}});
    if (!data) return make_error(data.error().code, data.error().message);

    Model::MutationResult result;
    result.accepted = true;
    result.message = "研讨室预约提交成功";
    result.summary = make_record(json_string(*data, "id"), theme, "reserved", {{"raw", data->dump()}});
    return result;
}

Result<Model::MutationResult> VenueReservationService::cancel_order(const std::string &order_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (order_id.empty()) return make_error(ErrorCode::InvalidArgument, "cgyy order cancel 需要 --order-id");
    auto parsed_order_id = parse_numeric_id(order_id, "--order-id <id>");
    if (!parsed_order_id) return make_error(parsed_order_id.error().code, parsed_order_id.error().message);
    auto data = request_json(HttpMethod::Post, "/api/orders/new/cancel/" + order_id);
    if (!data) return make_error(data.error().code, data.error().message);
    Model::MutationResult result;
    result.accepted = true;
    result.message = "研讨室预约已取消";
    result.summary = make_record(order_id, "研讨室预约取消", "cancelled", {{"raw", data->dump()}});
    return result;
}

} // namespace UBAANext
