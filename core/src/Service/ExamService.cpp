/**
 * @file ExamService.cpp
 * @brief 考试日程服务实现
 */

#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Parser/JsonParser.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>

#include <nlohmann/json.hpp>

namespace UBAANext {

namespace {

static constexpr const char *kCacheKeyExams = "cache:exam:list";
static constexpr int kCacheTtlSeconds = 300;
static constexpr const char *kExamUrl = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/student/exams.do";

Result<std::vector<Model::Exam>> parse_real_exams(const std::string &body) {
    try {
        auto json = nlohmann::json::parse(body);
        if (json.value("code", "") != "0") {
            return make_error(ErrorCode::NetworkError, "考试 API 返回错误: " + body.substr(0, 200));
        }

        std::vector<Model::Exam> exams;
        if (json.contains("datas") && json["datas"].is_array()) {
            for (const auto &item : json["datas"]) {
                Model::Exam exam;
                exam.id = item.value("taskId", "");
                exam.course_name = item.value("courseName", "");
                exam.course_no = item.value("courseNo", "");
                exam.location = item.value("examPlace", "");
                exam.time_text = item.value("examTimeDescription", "");
                exam.exam_date = item.value("examDate", "");
                exam.start_time = item.value("startTime", "");
                exam.end_time = item.value("endTime", "");
                exam.seat_no = item.value("examSeatNo", "");
                exam.exam_type = item.value("examType", "");
                int status = item.value("examStatus", 0);
                if (status >= 0 && status <= 2) {
                    exam.status = static_cast<Model::ExamStatus>(status);
                }
                exams.push_back(std::move(exam));
            }
        }
        return exams;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析考试 JSON 失败: ") + e.what());
    }
}

} // namespace

ExamService::ExamService(IHttpClient &http_client, ICacheStore &cache)
    : m_http_client(http_client), m_cache(cache), m_mode(ConnectionMode::Mock) {}

ExamService::ExamService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<std::vector<Model::Exam>> ExamService::get_exams(const std::string &term_code) {
    std::string cache_key = std::string(kCacheKeyExams) + ":" + term_code;
    auto cached = m_cache.get(cache_key);
    if (m_mode == ConnectionMode::Mock && cached.has_value()) {
        return Parser::parse_exams(*cached);
    }

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        auto activate_result = Protocol::Byxt::ensure_session(m_http_client, m_mode);
        if (!activate_result) {
            return make_error(activate_result.error().code, "激活考试会话失败: " + activate_result.error().message);
        }

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = Protocol::Byxt::resolve_url(std::string(kExamUrl) + "?termCode=" + term_code, m_mode);
        Protocol::Byxt::apply_ajax_headers(req, m_mode, "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/home/index.html");
        req.headers["Accept"] = "*/*";

        auto response_result = m_http_client.send(req);
        if (!response_result) {
            return make_error(ErrorCode::NetworkError, "请求考试列表失败: " + response_result.error().message);
        }
        const auto &response = *response_result;
        if (Protocol::Byxt::is_session_expired_response(response)) {
            return make_error(ErrorCode::SessionExpired, "BYXT 会话已过期");
        }
        if (response.status_code != 200) {
            return make_error(ErrorCode::NetworkError, "考试请求返回: " + std::to_string(response.status_code));
        }
        return parse_real_exams(response.body);
    }

    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/exam/list";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_exams(response.body);
    if (parsed.has_value()) {
        m_cache.set_with_ttl(cache_key, response.body, kCacheTtlSeconds);
    }

    return parsed;
}

} // namespace UBAANext
