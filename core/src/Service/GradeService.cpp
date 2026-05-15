#include <UBAANext/Service/GradeService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Protocol/ScoreSession.hpp>

#include <nlohmann/json.hpp>

#include <exception>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>

namespace UBAANext {

namespace {

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

std::string parse_score_xq(const std::string &term_code) {
    auto last_dash = term_code.rfind('-');
    if (last_dash == std::string::npos) {
        return "";
    }
    auto term = term_code.substr(last_dash + 1);
    return term == "1" ? "1" : term == "2" ? "2" : "3";
}

std::string parse_score_year(const std::string &term_code) {
    auto first_dash = term_code.find('-');
    if (first_dash == std::string::npos) {
        return term_code;
    }
    return term_code.substr(0, first_dash);
}

std::string json_to_string(const nlohmann::json &value) {
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number_integer()) return std::to_string(value.get<int>());
    if (value.is_number_unsigned()) return std::to_string(value.get<unsigned int>());
    if (value.is_number_float()) return std::to_string(value.get<double>());
    if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
    return {};
}

std::string pick_field(const nlohmann::json &value, std::initializer_list<const char *> keys) {
    if (!value.is_object()) return {};
    for (const auto *key : keys) {
        auto it = value.find(key);
        if (it != value.end()) {
            auto text = json_to_string(*it);
            if (!text.empty()) return text;
        }
    }
    return {};
}

Model::Grade parse_grade(const std::string &id, const nlohmann::json &value, const std::string &term_code) {
    Model::Grade grade;
    grade.id = id;
    grade.term_code = term_code;
    grade.course_name = pick_field(value, {"kcmc", "courseName", "name"});
    grade.course_code = pick_field(value, {"kch", "courseCode", "code"});
    grade.course_type = pick_field(value, {"kclb", "courseType", "type"});
    grade.credit = pick_field(value, {"xf", "credit", "credits"});
    grade.score = pick_field(value, {"cj", "score", "grade"});
    grade.grade_point = pick_field(value, {"jd", "gradePoint", "gpa"});
    grade.raw_status = pick_field(value, {"zt", "status"});
    return grade;
}

Result<std::vector<Model::Grade>> parse_grades_response(const std::string &body, const std::string &term_code) {
    try {
        auto json = nlohmann::json::parse(body);
        if (json.value("code", 0) != 0) {
            return make_error(ErrorCode::NetworkError, "成绩 API 返回错误");
        }

        std::vector<Model::Grade> grades;
        if (!json.contains("data")) {
            return grades;
        }

        const auto &data = json["data"];
        if (data.is_object()) {
            for (const auto &[key, value] : data.items()) {
                if (value.is_array()) {
                    for (std::size_t i = 0; i < value.size(); ++i) {
                        auto grade = parse_grade(key + ":" + std::to_string(i), value[i], term_code.empty() ? key : term_code);
                        if (!grade.course_name.empty() || !grade.score.empty()) grades.push_back(std::move(grade));
                    }
                } else {
                    auto grade = parse_grade(key, value, term_code);
                    if (!grade.course_name.empty() || !grade.score.empty()) grades.push_back(std::move(grade));
                }
            }
        } else if (data.is_array()) {
            for (std::size_t i = 0; i < data.size(); ++i) {
                auto grade = parse_grade(std::to_string(i + 1), data[i], term_code);
                if (!grade.course_name.empty() || !grade.score.empty()) grades.push_back(std::move(grade));
            }
        }
        return grades;
    } catch (const std::exception &e) {
        return make_error(ErrorCode::ParseError, std::string("解析成绩 JSON 失败: ") + e.what());
    }
}

} // namespace

GradeService::GradeService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {
    (void)m_cache;
}

Result<std::vector<Model::Grade>> GradeService::list_grades(const std::string &term_code) {
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        return std::vector<Model::Grade>{{"grade-1", "高等数学", "MATH101", "必修", "4", "95", "4.0", term_code.empty() ? "2025-2026-2" : term_code, "posted"}};
    }
#endif

    auto session = Protocol::Score::ensure_session(m_http_client, m_mode);
    if (!session) {
        return make_error(session.error().code, "激活成绩系统失败: " + session.error().message);
    }

    HttpRequest request;
    request.method = HttpMethod::Post;
    request.url = resolve_for_mode("https://app.buaa.edu.cn/buaascore/wap/default/index", m_mode);
    Protocol::Score::apply_form_headers(request);
    request.body = "xq=" + parse_score_xq(term_code) + "&year=" + parse_score_year(term_code);

    auto response = m_http_client.send(request);
    if (!response) {
        return make_error(ErrorCode::NetworkError, "请求成绩失败: " + response.error().message);
    }
    if (Protocol::Score::is_session_expired_response(*response)) {
        return make_error(ErrorCode::SessionExpired, "成绩系统会话已过期");
    }
    if (response->status_code != 200) {
        return make_error(ErrorCode::NetworkError, "成绩请求返回: " + std::to_string(response->status_code));
    }

    return parse_grades_response(response->body, term_code);
}

Result<std::vector<Model::Grade>> GradeService::list_all_grades() {
#if UBAANEXT_ENABLE_MOCKS
    if (m_mode == ConnectionMode::Mock) {
        return std::vector<Model::Grade>{
            {"grade-1", "高等数学", "MATH101", "必修", "4", "95", "4.0", "2025-2026-2", "posted"},
            {"grade-2", "大学物理", "PHYS101", "必修", "3", "90", "3.7", "2025-2026-1", "posted"},
        };
    }
#endif

    return list_grades("");
}

} // namespace UBAANext
