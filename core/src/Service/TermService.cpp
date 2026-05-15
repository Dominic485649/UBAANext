/**
 * @file TermService.cpp
 * @brief 学期/周次服务实现
 */

#include <UBAANext/Service/TermService.hpp>
#include <UBAANext/Parser/JsonParser.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>

#include <nlohmann/json.hpp>

namespace UBAANext {

namespace {

static constexpr const char *kCacheKeyTerms = "cache:term:list";
static constexpr int kCacheTtlSeconds = 300;
static constexpr const char *kTermsUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/student/schoolCalendars.do";
static constexpr const char *kWeeksUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/getTermWeeks.do";

Result<std::vector<Model::Term>> parse_real_terms(const std::string &body) {
    try {
        auto json = nlohmann::json::parse(body);
        if (json.value("code", "") != "0") {
            return make_error(ErrorCode::NetworkError, "学期 API 返回错误: " + body.substr(0, 200));
        }

        std::vector<Model::Term> terms;
        if (json.contains("datas") && json["datas"].is_array()) {
            for (const auto &item : json["datas"]) {
                Model::Term term;
                term.code = item.value("itemCode", "");
                term.name = item.value("itemName", "");
                term.selected = item.value("selected", false);
                term.index = item.value("itemIndex", 0);
                terms.push_back(std::move(term));
            }
        }
        return terms;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析学期 JSON 失败: ") + e.what());
    }
}

Result<std::vector<Model::Week>> parse_real_weeks(const std::string &body) {
    try {
        auto json = nlohmann::json::parse(body);
        if (json.value("code", "") != "0") {
            return make_error(ErrorCode::NetworkError, "周次 API 返回错误: " + body.substr(0, 200));
        }

        std::vector<Model::Week> weeks;
        if (json.contains("datas") && json["datas"].is_array()) {
            for (const auto &item : json["datas"]) {
                Model::Week week;
                week.serial_number = item.value("serialNumber", 0);
                week.name = item.value("name", "");
                week.start_date = item.value("startDate", "");
                week.end_date = item.value("endDate", "");
                week.is_current = item.value("curWeek", false);
                weeks.push_back(std::move(week));
            }
        }
        return weeks;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析周次 JSON 失败: ") + e.what());
    }
}

} // namespace

#if UBAANEXT_ENABLE_MOCKS
TermService::TermService(IHttpClient &http_client, ICacheStore &cache)
    : m_http_client(http_client), m_cache(cache), m_mode(ConnectionMode::Mock) {}
#endif

TermService::TermService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::vector<Model::Term>> TermService::get_terms() {
#if UBAANEXT_ENABLE_MOCKS
    auto cached = m_cache.get(kCacheKeyTerms);
    if (m_mode == ConnectionMode::Mock && cached.has_value()) {
        return Parser::parse_terms(*cached);
    }
#endif

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        auto activate_result = Protocol::Byxt::ensure_session(m_http_client, m_mode);
        if (!activate_result) {
            return make_error(activate_result.error().code, "激活学期会话失败: " + activate_result.error().message);
        }

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = Protocol::Byxt::resolve_url(kTermsUrl, m_mode);
        Protocol::Byxt::apply_ajax_headers(req, m_mode);

        auto response_result = m_http_client.send(req);
        if (!response_result) {
            return make_error(ErrorCode::NetworkError, "请求学期列表失败: " + response_result.error().message);
        }
        const auto &response = *response_result;
        if (Protocol::Byxt::is_session_expired_response(response)) {
            return make_error(ErrorCode::SessionExpired, "BYXT 会话已过期");
        }
        if (response.status_code != 200) {
            return make_error(ErrorCode::NetworkError, "学期请求返回: " + std::to_string(response.status_code));
        }
        return parse_real_terms(response.body);
    }

#if UBAANEXT_ENABLE_MOCKS
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/schedule/terms";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_terms(response.body);
    if (parsed.has_value()) {
        m_cache.set_with_ttl(kCacheKeyTerms, response.body, kCacheTtlSeconds);
    }

    return parsed;
#else
    return make_error(ErrorCode::InvalidArgument, "学期查询需要 Direct 或 WebVPN 连接模式");
#endif
}

Result<std::vector<Model::Week>> TermService::get_weeks(const std::string &term_code) {
    std::string cache_key = "cache:week:" + term_code;

#if UBAANEXT_ENABLE_MOCKS
    auto cached = m_cache.get(cache_key);
    if (m_mode == ConnectionMode::Mock && cached.has_value()) {
        return Parser::parse_weeks(*cached);
    }
#endif

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        auto activate_result = Protocol::Byxt::ensure_session(m_http_client, m_mode);
        if (!activate_result) {
            return make_error(activate_result.error().code, "激活周次会话失败: " + activate_result.error().message);
        }

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = Protocol::Byxt::resolve_url(std::string(kWeeksUrl) + "?termCode=" + term_code, m_mode);
        Protocol::Byxt::apply_ajax_headers(req, m_mode);

        auto response_result = m_http_client.send(req);
        if (!response_result) {
            return make_error(ErrorCode::NetworkError, "请求周次列表失败: " + response_result.error().message);
        }
        const auto &response = *response_result;
        if (Protocol::Byxt::is_session_expired_response(response)) {
            return make_error(ErrorCode::SessionExpired, "BYXT 会话已过期");
        }
        if (response.status_code != 200) {
            return make_error(ErrorCode::NetworkError, "周次请求返回: " + std::to_string(response.status_code));
        }
        return parse_real_weeks(response.body);
    }

#if UBAANEXT_ENABLE_MOCKS
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/schedule/weeks";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_weeks(response.body);
    if (parsed.has_value()) {
        m_cache.set_with_ttl(cache_key, response.body, kCacheTtlSeconds);
    }

    return parsed;
#else
    (void)cache_key;
    return make_error(ErrorCode::InvalidArgument, "周次查询需要 Direct 或 WebVPN 连接模式");
#endif
}

} // namespace UBAANext
