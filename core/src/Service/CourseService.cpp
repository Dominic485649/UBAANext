/**
 * @file CourseService.cpp
 * @brief 课程表服务实现（支持 mock 和真实 API）
 */

#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Parser/JsonParser.hpp>
#include <UBAANext/Protocol/ByxtSession.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>

namespace UBAANext {

static constexpr int kCacheTtlSeconds = 300;

static const char *BYXT_WEEKLY_SCHEDULE = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/student/getMyScheduleDetail.do";
static const char *BYXT_TODAY_SCHEDULE = "https://byxt.buaa.edu.cn/jwapp/sys/homeapp/api/home/teachingSchedule/detail.do";

void apply_schedule_headers(HttpRequest &req, ConnectionMode mode) {
    req.headers["Accept"] = "application/json, text/javascript, */*; q=0.01";
    req.headers["X-Requested-With"] = "XMLHttpRequest";
    req.headers["Referer"] = Protocol::Byxt::resolve_url("https://byxt.buaa.edu.cn/jwapp/sys/homeapp/index.html", mode);
    req.headers["User-Agent"] = "UBAANext/0.4";
}

#if UBAANEXT_ENABLE_MOCKS
CourseService::CourseService(IHttpClient &http_client, ICacheStore &cache)
    : m_http_client(http_client), m_cache(cache), m_mode(ConnectionMode::Mock) {}
#endif

CourseService::CourseService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

std::string CourseService::resolve_url(const std::string &url) const {
    return Protocol::Byxt::resolve_url(url, m_mode);
}

Result<std::vector<Model::Course>> CourseService::get_today_courses() {
    // 使用真实日期
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    char buf[11];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return get_date_courses(buf);
}

Result<std::vector<Model::Course>> CourseService::get_date_courses(const std::string &date) {
    std::string cache_key = "cache:course:date:" + date;
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        auto cached = m_cache.get(cache_key);
        if (cached.has_value()) {
            return Parser::parse_courses(*cached);
        }
    }
#endif

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        auto activate_result = Protocol::Byxt::ensure_session(m_http_client, m_mode);
        if (!activate_result) {
            return UBAANext::make_error(activate_result.error().code, "激活课表会话失败: " + activate_result.error().message);
        }

        HttpRequest req;
        req.method = HttpMethod::Get;
        req.url = resolve_url(std::string(BYXT_TODAY_SCHEDULE) + "?rq=" + date + "&lxdm=student");
        apply_schedule_headers(req, m_mode);

        auto response_result = m_http_client.send(req);
        if (!response_result) {
            return UBAANext::make_error(ErrorCode::NetworkError, "请求日期课表失败: " + response_result.error().message);
        }
        const auto &response = *response_result;
        if (response.status_code != 200) {
            return UBAANext::make_error(ErrorCode::NetworkError, "日期课表请求返回: " + std::to_string(response.status_code));
        }

        try {
            auto json = nlohmann::json::parse(response.body);
            if (json.value("code", "") != "0") {
                return UBAANext::make_error(ErrorCode::NetworkError, "日期课表 API 返回错误: code=" + json.value("code", "?"));
            }

            std::vector<Model::Course> courses;
            if (json.contains("datas") && json["datas"].is_array()) {
                int index = 0;
                for (const auto &item : json["datas"]) {
                    Model::Course c;
                    c.name = item.value("bizName", "");
                    c.classroom = item.value("place", "");
                    c.teacher = item.value("teacher", "");
                    c.day_of_week = 0;
                    c.week_start = 1;
                    c.week_end = 20;
                    c.course_code = item.value("shortName", "");
                    std::string time = item.value("time", "");
                    auto dash = time.find('-');
                    if (dash != std::string::npos) {
                        c.begin_time = time.substr(0, dash);
                        c.end_time = time.substr(dash + 1);
                    }
                    c.section_start = ++index;
                    c.section_end = index;
                    courses.push_back(std::move(c));
                }
            }
            return courses;
        } catch (const std::exception &e) {
            return UBAANext::make_error(ErrorCode::ParseError, std::string("解析日期课表 JSON 失败: ") + e.what());
        }
    }

#if UBAANEXT_ENABLE_MOCKS
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/schedule/today";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return UBAANext::make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return UBAANext::make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_courses(response.body);
    if (parsed.has_value()) {
        m_cache.set_with_ttl(cache_key, response.body, kCacheTtlSeconds);
    }
    return parsed;
#else
    (void)cache_key;
    return UBAANext::make_error(ErrorCode::InvalidArgument, "课程查询需要 Direct 或 WebVPN 连接模式");
#endif
}

Result<std::vector<Model::Course>> CourseService::get_week_courses(int week) {
    std::string cache_key = "cache:course:week:" + std::to_string(week);

    auto cached = m_cache.get(cache_key);
    if (cached.has_value()) {
        auto parsed = Parser::parse_courses(*cached);
        if (parsed.has_value()) {
            return parsed;
        }
    }

    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        return fetch_week_courses_real(week, "2025-2026-2");
    }

#if UBAANEXT_ENABLE_MOCKS
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url = "/schedule/week";

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return UBAANext::make_error(ErrorCode::NetworkError, response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return UBAANext::make_error(ErrorCode::NetworkError,
                          "HTTP 请求失败: 状态码 " + std::to_string(response.status_code));
    }

    auto parsed = Parser::parse_courses(response.body);
    if (!parsed.has_value()) {
        return parsed;
    }

    m_cache.set_with_ttl(cache_key, response.body, kCacheTtlSeconds);
    return parsed;
#else
    (void)cache_key;
    return UBAANext::make_error(ErrorCode::InvalidArgument, "课程查询需要 Direct 或 WebVPN 连接模式");
#endif
}

Result<std::vector<Model::Course>> CourseService::get_week_courses(int week, const std::string &term_code) {
    if (m_mode == ConnectionMode::Direct || m_mode == ConnectionMode::WebVPN) {
        return fetch_week_courses_real(week, term_code);
    }
#if UBAANEXT_ENABLE_MOCKS
    return get_week_courses(week);
#else
    return UBAANext::make_error(ErrorCode::InvalidArgument, "课程查询需要 Direct 或 WebVPN 连接模式");
#endif
}

Result<std::vector<Model::Course>> CourseService::fetch_week_courses_real(int week, const std::string &term_code) {
    auto activate_result = Protocol::Byxt::ensure_session(m_http_client, m_mode);
    if (!activate_result) {
        return UBAANext::make_error(activate_result.error().code, "激活周课表会话失败: " + activate_result.error().message);
    }

    HttpRequest req;
    req.method = HttpMethod::Post;
    req.url = resolve_url(BYXT_WEEKLY_SCHEDULE);
    req.headers["Content-Type"] = "application/x-www-form-urlencoded";
    apply_schedule_headers(req, m_mode);

    int actual_week = (week > 0) ? week : 8;
    req.body = "termCode=" + term_code + "&type=week&week=" + std::to_string(actual_week);

    auto response_result = m_http_client.send(req);
    if (!response_result) {
        return UBAANext::make_error(ErrorCode::NetworkError, "请求课表失败: " + response_result.error().message);
    }
    const auto &response = *response_result;
    if (response.status_code != 200) {
        return UBAANext::make_error(ErrorCode::NetworkError,
                          "课表请求返回: " + std::to_string(response.status_code));
    }

    try {
        auto json = nlohmann::json::parse(response.body);
        if (json.value("code", "") != "0") {
            return UBAANext::make_error(ErrorCode::NetworkError, "周课表 API 返回错误: code=" + json.value("code", "?") + " msg=" + json.value("msg", "none"));
        }

        auto &datas = json["datas"];
        if (!datas.contains("arrangedList") || !datas["arrangedList"].is_array()) {
            return std::vector<Model::Course>{};
        }

        auto string_or_empty = [](const nlohmann::json &item, const char *key) {
            return item.contains(key) && item[key].is_string() ? item[key].get<std::string>() : std::string{};
        };

        std::vector<Model::Course> courses;
        for (auto &item : datas["arrangedList"]) {
            Model::Course c;
            c.name = string_or_empty(item, "courseName");
            c.teacher = string_or_empty(item, "weeksAndTeachers");
            auto space_pos = c.teacher.find(' ');
            if (space_pos != std::string::npos) {
                c.teacher = c.teacher.substr(space_pos + 1);
            }
            c.classroom = string_or_empty(item, "placeName");
            c.course_code = string_or_empty(item, "courseCode");
            c.credit = string_or_empty(item, "credit");
            c.begin_time = string_or_empty(item, "beginTime");
            c.end_time = string_or_empty(item, "endTime");
            c.section_start = item.value("beginSection", 0);
            c.section_end = item.value("endSection", 0);
            c.day_of_week = item.value("dayOfWeek", 0);

            std::string wt = string_or_empty(item, "weeksAndTeachers");
            auto dash = wt.find('-');
            auto zhou = wt.find("周");
            if (dash != std::string::npos && zhou != std::string::npos) {
                c.week_start = std::stoi(wt.substr(0, dash));
                c.week_end = std::stoi(wt.substr(dash + 1, zhou - dash - 1));
            } else {
                c.week_start = 1;
                c.week_end = 16;
            }

            if (week > 0 && (week < c.week_start || week > c.week_end)) {
                continue;
            }

            courses.push_back(std::move(c));
        }

        return courses;
    } catch (const std::exception &e) {
        return UBAANext::make_error(ErrorCode::ParseError, std::string("解析课表 JSON 失败: ") + e.what());
    }
}

} // namespace UBAANext
