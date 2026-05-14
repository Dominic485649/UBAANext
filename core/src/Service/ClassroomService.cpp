/**
 * @file ClassroomService.cpp
 * @brief 教室可用性服务实现
 */

#include <UBAANext/Service/ClassroomService.hpp>
#include <UBAANext/Parser/JsonParser.hpp>
#include <UBAANext/Protocol/AppBuaaSession.hpp>

#include <nlohmann/json.hpp>

#include <sstream>
#include <vector>

namespace UBAANext {

namespace {

static constexpr int kCacheTtlSeconds = 300;
static constexpr const char *kClassroomSyncUrl = "https://sso.buaa.edu.cn/login?service=https%3A%2F%2Fapp.buaa.edu.cn%2Fa_buaa%2Fapi%2Fcas%2Findex%3Fredirect%3Dhttps%253A%252F%252Fapp.buaa.edu.cn%252Fsite%252FclassRoomQuery%252Findex%26from%3Dwap%26login_from%3D&noAutoRedirect=1";
static constexpr const char *kClassroomQueryUrl = "https://app.buaa.edu.cn/buaafreeclass/wap/default/search1";
static constexpr const char *kMobileUserAgent = "Mozilla/5.0 (Linux; Android 16; 24031PN0DC Build/BP2A.250605.031.A3; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/138.0.7204.180 Mobile Safari/537.36 XWEB/1380275 MMWEBSDK/20230806 MMWEBID/4102 wxworklocal/3.2.200 wwlocal/3.2.200 wxwork/4.0.0 appname/wxworklocal-customized wxworklocal-device-code/195ef5586d7d3c2808fcbea32d77c0d4 MicroMessenger/7.0.1 appScheme/wxworklocalcustomized Language/zh_CN ColorScheme/Light WXWorklocalClientType/Android Brand/xiaomi";

void apply_classroom_headers(HttpRequest &req, ConnectionMode mode) {
    Protocol::AppBuaa::apply_ajax_headers(req, mode, "https://app.buaa.edu.cn/site/classRoomQuery/index", kMobileUserAgent);
}

std::vector<int> parse_sections(const std::string &value) {
    std::vector<int> sections;
    std::istringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            sections.push_back(std::stoi(item));
        } catch (...) {
        }
    }
    return sections;
}

bool session_expired_body(const std::string &body) {
    return body.find("统一身份认证") != std::string::npos ||
           body.find("input name=\"execution\"") != std::string::npos ||
           body.find("未登录") != std::string::npos ||
           body.find("login") != std::string::npos;
}

Result<Model::ClassroomQueryResult> parse_real_classrooms(const std::string &body) {
    try {
        auto json = nlohmann::json::parse(body);
        if (json.value("e", 0) != 0 && json.value("e", 0) != 1) {
            return make_error(ErrorCode::NetworkError, "空教室 API 返回错误: " + body.substr(0, 200));
        }

        Model::ClassroomQueryResult result;
        if (!json.contains("d") || !json["d"].contains("list") || !json["d"]["list"].is_object()) {
            return result;
        }

        for (auto &[building, rooms] : json["d"]["list"].items()) {
            if (!rooms.is_array()) {
                continue;
            }
            std::vector<Model::ClassroomInfo> room_list;
            for (const auto &room : rooms) {
                Model::ClassroomInfo info;
                info.id = room.value("id", "");
                info.floor_id = room.value("floorid", "");
                info.name = room.value("name", "");
                info.free_sections = parse_sections(room.value("kxsds", ""));
                room_list.push_back(std::move(info));
            }
            result.buildings[building] = std::move(room_list);
        }
        return result;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析空教室 JSON 失败: ") + e.what());
    }
}

} // namespace

ClassroomService::ClassroomService(IHttpClient &http_client, ICacheStore &cache)
    : m_http_client(http_client), m_cache(cache), m_mode(ConnectionMode::Mock) {}

ClassroomService::ClassroomService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<Model::ClassroomQueryResult>
ClassroomService::query_classrooms(int campus_id, const std::string &date) {
    return query_classrooms(campus_id, date, {}, {});
}

Result<Model::ClassroomQueryResult>
ClassroomService::query_classrooms(int campus_id,
                                   const std::string &date,
                                   const std::string &username,
                                   const std::string &password) {
    std::string cache_key = "cache:classroom:" + std::to_string(campus_id) + ":" + date;

    auto cached = m_cache.get(cache_key);
    if (m_mode == ConnectionMode::Mock && cached.has_value()) {
        return Parser::parse_classrooms(*cached);
    }

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        auto session = username.empty() || password.empty()
            ? Protocol::AppBuaa::ensure_session(m_http_client, m_mode, kClassroomSyncUrl, kMobileUserAgent)
            : Protocol::AppBuaa::ensure_session(m_http_client, m_mode, kClassroomSyncUrl, kMobileUserAgent, username, password);
        if (!session) {
            return make_error(session.error().code, "激活空教室会话失败: " + session.error().message);
        }

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = Protocol::AppBuaa::resolve_url(std::string(kClassroomQueryUrl) + "?xqid=" + std::to_string(campus_id) + "&floorid=&date=" + date, m_mode);
        apply_classroom_headers(req, m_mode);

        auto response_result = m_http_client.send(req);
        if (!response_result) {
            return make_error(ErrorCode::NetworkError, "请求空教室失败: " + response_result.error().message);
        }
        const auto &response = *response_result;
        if (response.status_code == 401 || response.status_code == 403 || session_expired_body(response.body)) {
            auto prefix = response.body.substr(0, std::min<std::size_t>(response.body.size(), 180));
            return make_error(ErrorCode::SessionExpired,
                              "空教室会话已过期: status=" + std::to_string(response.status_code) + " body=" + prefix);
        }
        if (response.status_code != 200) {
            return make_error(ErrorCode::NetworkError, "空教室请求返回: " + std::to_string(response.status_code));
        }
        return parse_real_classrooms(response.body);
    }

    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/classroom/query";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_classrooms(response.body);
    if (parsed.has_value()) {
        m_cache.set_with_ttl(cache_key, response.body, kCacheTtlSeconds);
    }

    return parsed;
}

} // namespace UBAANext
