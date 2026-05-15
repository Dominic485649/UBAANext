#include <UBAANext/Service/EvaluationService.hpp>

#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/EvaluationParser.hpp>
#include <UBAANext/Service/ResponseUtils.hpp>

#include <iomanip>
#include <map>
#include <sstream>
#include <utility>
#include <set>

namespace UBAANext {

namespace {

std::string resolve_for_mode(const std::string &url, ConnectionMode mode) {
    return mode == ConnectionMode::WebVPN ? VpnCipher::to_vpn_url(url) : url;
}

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

void apply_headers(HttpRequest &request) {
    request.headers["Accept"] = "application/json, text/plain, */*";
    request.headers["Accept-Language"] = "zh-CN,zh;q=0.9";
    request.headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) UBAANext/0.4";
    request.headers["X-Requested-With"] = "XMLHttpRequest";
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

Model::FeatureRecord task_to_record(const Model::EvaluationTask &task) {
    return make_record(task.id, task.title, task.status, {
        {"teacher", task.teacher},
        {"rwid", task.task_id},
        {"wjid", task.questionnaire_id},
        {"kcdm", task.course_code},
        {"bpdm", task.teacher_code},
        {"xnxq", task.term_code},
        {"msid", task.pattern_id},
        {"ypjcs", std::to_string(task.evaluated_count)},
        {"xypjcs", std::to_string(task.required_count)},
    });
}

} // namespace

EvaluationService::EvaluationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

Result<void> EvaluationService::activate_session() {
    (void)m_cache;
    if (m_activated) return {};
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = resolve_for_mode("https://spoc.buaa.edu.cn/pjxt/cas", m_mode);
    apply_headers(request);
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "激活评教会话失败: " + response.error().message);
    if (ServiceResponse::is_session_expired_response(*response)) return make_error(ErrorCode::SessionExpired, "评教会话已过期，请重新登录");
    if (response->status_code < 200 || response->status_code >= 300) return make_error(ErrorCode::NetworkError, "评教会话激活返回: " + std::to_string(response->status_code));
    m_activated = true;
    return {};
}

Result<nlohmann::json> EvaluationService::request_json(HttpMethod method, const std::string &url, const nlohmann::json &body) {
    HttpRequest request;
    request.method = method;
    request.url = resolve_for_mode(url, m_mode);
    apply_headers(request);
    if (method == HttpMethod::Post) {
        request.headers["Content-Type"] = "application/json";
        if (!body.is_null()) request.body = body.dump();
    }
    auto response = m_http_client.send(request);
    if (!response) return make_error(ErrorCode::NetworkError, "请求评教失败: " + response.error().message);
    return ServiceResponse::parse_json_response(*response, "评教");
}

Result<std::string> EvaluationService::current_xnxq() {
    auto result = request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/component/queryXnxq");
    if (!result) return make_error(result.error().code, result.error().message);
    if (!result->is_array() || result->empty() || !(*result)[0].is_object()) return make_error(ErrorCode::ParseError, "评教学期响应为空");
    auto xn = json_string((*result)[0], "xn");
    auto xq = json_string((*result)[0], "xq");
    if (xn.empty() || xq.empty()) return make_error(ErrorCode::ParseError, "评教学期缺少 xn/xq 字段");
    return xn + xq;
}

Result<std::vector<Model::EvaluationTask>> EvaluationService::list_evaluation_tasks() {
    auto activation = activate_session();
    if (!activation) return make_error(activation.error().code, activation.error().message);
    auto xnxq = current_xnxq();
    if (!xnxq) return make_error(xnxq.error().code, xnxq.error().message);

    auto tasks_data = request_json(HttpMethod::Get, "https://spoc.buaa.edu.cn/pjxt/personnelEvaluation/listObtainPersonnelEvaluationTasks?rwmc=&sfyp=0&pageNum=1&pageSize=10");
    if (!tasks_data) return make_error(tasks_data.error().code, tasks_data.error().message);
    auto tasks = tasks_data->contains("list") && (*tasks_data)["list"].is_array() ? (*tasks_data)["list"] : nlohmann::json::array();

    std::map<std::string, Model::EvaluationTask> records;
    for (const auto &task : tasks) {
        auto rwid = json_string(task, "rwid");
        if (rwid.empty()) continue;
        auto questionnaires_data = request_json(HttpMethod::Get, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireListToTask?rwid=" + url_encode(rwid) + "&sfyp=0&pageNum=1&pageSize=999");
        if (!questionnaires_data || !questionnaires_data->is_array()) continue;
        for (const auto &questionnaire : *questionnaires_data) {
            auto wjid = json_string(questionnaire, "wjid");
            auto msid = json_string(questionnaire, "msid");
            if (msid.empty()) msid = "1";
            if (wjid.empty()) continue;
            (void)request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern", nlohmann::json{{"rwid", rwid}, {"wjid", wjid}, {"msid", msid}});
            for (const auto &sfyp : {std::string("0"), std::string("1")}) {
                auto courses_data = request_json(HttpMethod::Get, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=" + sfyp + "&wjid=" + url_encode(wjid) + "&xnxq=" + url_encode(*xnxq) + "&pageNum=1&pageSize=999");
                if (!courses_data || !courses_data->is_array()) continue;
                auto status = sfyp == "1" ? "evaluated" : "pending";
                for (auto record : Parser::parse_evaluation_required_reviews(*courses_data, rwid, wjid, msid, *xnxq, status)) {
                    records[record.id] = std::move(record);
                }
            }
        }
    }

    std::vector<Model::EvaluationTask> out;
    out.reserve(records.size());
    for (auto &[_, record] : records) out.push_back(std::move(record));
    return out;
}

Result<std::vector<Model::FeatureRecord>> EvaluationService::list_evaluations() {
    auto result = list_evaluation_tasks();
    if (!result) return make_error(result.error().code, result.error().message);
    std::vector<Model::FeatureRecord> records;
    for (const auto &task : *result) records.push_back(task_to_record(task));
    return records;
}

Result<Model::MutationResult> EvaluationService::submit_evaluations(const std::string &target_id) {
    auto records = list_evaluations();
    if (!records) return make_error(records.error().code, records.error().message);

    std::vector<Model::FeatureRecord> targets;
    for (const auto &record : *records) {
        if (record.status != "pending") continue;
        auto course_it = record.fields.find("kcdm");
        if (!target_id.empty() && record.id != target_id && (course_it == record.fields.end() || course_it->second != target_id)) continue;
        targets.push_back(record);
    }
    if (targets.empty()) return make_error(ErrorCode::InvalidArgument, target_id.empty() ? "没有待提交的评教课程" : "未找到指定待评教课程: " + target_id);

    nlohmann::json results = nlohmann::json::array();
    int success_count = 0;
    for (const auto &course : targets) {
        auto rwid = course.fields.at("rwid");
        auto wjid = course.fields.at("wjid");
        auto msid = course.fields.at("msid");
        (void)request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern", nlohmann::json{{"rwid", rwid}, {"wjid", wjid}, {"msid", msid}});

        auto topic_url = "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireTopic?id=&rwid=" + url_encode(rwid) +
                         "&wjid=" + url_encode(wjid) +
                         "&zdmc=STID&ypjcs=" + course.fields.at("ypjcs") +
                         "&xypjcs=" + course.fields.at("xypjcs") +
                         "&sxz=&pjrdm=&pjrmc=&bpdm=" + url_encode(course.fields.at("bpdm")) +
                         "&bpmc=" + url_encode(course.fields.at("teacher")) +
                         "&kcdm=" + url_encode(course.fields.at("kcdm")) +
                         "&kcmc=" + url_encode(course.title) +
                         "&rwh=&xn=&xq=&xnxq=" + url_encode(course.fields.at("xnxq")) +
                         "&pjlxid=2&sfksqbpj=1&yxsfktjst=&yxdm=";
        auto topic_data = request_json(HttpMethod::Get, topic_url);
        if (!topic_data) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", topic_data.error().message}});
            continue;
        }
        auto topic = topic_data->is_array() && !topic_data->empty() ? (*topic_data)[0] : *topic_data;
        auto questionnaire = topic.contains("pjxtWjWjbReturnEntity") && topic["pjxtWjWjbReturnEntity"].is_object() ? topic["pjxtWjWjbReturnEntity"] : nlohmann::json::object();
        auto sections = questionnaire.contains("wjzblist") && questionnaire["wjzblist"].is_array() ? questionnaire["wjzblist"] : nlohmann::json::array();
        std::vector<nlohmann::json> questions;
        for (const auto &section : sections) {
            if (!section.is_object() || !section.contains("tklist") || !section["tklist"].is_array()) continue;
            for (const auto &question : section["tklist"]) if (question.is_object()) questions.push_back(question);
        }
        if (questions.empty()) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", "问卷没有题目"}});
            continue;
        }

        auto pjjgckb = topic.contains("pjxtPjjgPjjgckb") && topic["pjxtPjjgPjjgckb"].is_array() ? topic["pjxtPjjgPjjgckb"] : nlohmann::json::array();
        nlohmann::json pjjglist = nlohmann::json::array();
        for (const auto &entry : pjjgckb) {
            if (!entry.is_object()) continue;
            nlohmann::json pjxxlist = nlohmann::json::array();
            for (size_t i = 0; i < questions.size(); ++i) {
                const auto &question = questions[i];
                auto options = question.contains("tmxxlist") && question["tmxxlist"].is_array() ? question["tmxxlist"] : nlohmann::json::array();
                nlohmann::json answer_ids = nlohmann::json::array();
                if (!options.empty()) {
                    auto index = i == 0 && options.size() > 1 ? 1 : 0;
                    if (options[index].is_object() && options[index].contains("tmxxid")) answer_ids.push_back(options[index]["tmxxid"]);
                }
                auto question_type = json_string(question, "tmlx").empty() ? "1" : json_string(question, "tmlx");
                pjxxlist.push_back({{"sjly", "1"}, {"stlx", question_type}, {"wjid", wjid}, {"wjssrwid", entry.value("wjssrwid", nlohmann::json(nullptr))}, {"wjstctid", question_type == "6" && !options.empty() && options[0].is_object() ? options[0].value("tmxxid", nlohmann::json("")) : nlohmann::json("")}, {"wjstid", question.value("tmid", nlohmann::json(nullptr))}, {"xxdalist", answer_ids}});
            }
            pjjglist.push_back({{"bprdm", entry.value("bprdm", nlohmann::json(nullptr))}, {"bprmc", entry.value("bprmc", nlohmann::json(nullptr))}, {"kcdm", entry.value("kcdm", nlohmann::json(nullptr))}, {"kcmc", entry.value("kcmc", nlohmann::json(nullptr))}, {"pjdf", 93}, {"pjfs", entry.value("pjfs", nlohmann::json("1"))}, {"pjid", entry.value("pjid", nlohmann::json(nullptr))}, {"pjlx", entry.value("pjlx", nlohmann::json(nullptr))}, {"pjmap", topic.value("pjmap", nlohmann::json(nullptr))}, {"pjrdm", entry.value("pjrdm", nlohmann::json(nullptr))}, {"pjrjsdm", entry.value("pjrjsdm", nlohmann::json(nullptr))}, {"pjrxm", entry.value("pjrxm", nlohmann::json(nullptr))}, {"pjsx", 1}, {"pjxxlist", pjxxlist}, {"rwh", entry.value("rwh", nlohmann::json(nullptr))}, {"stzjid", "xx"}, {"wjid", wjid}, {"wjssrwid", entry.value("wjssrwid", nlohmann::json(nullptr))}, {"wtjjy", ""}, {"xhgs", nullptr}, {"xnxq", entry.value("xnxq", nlohmann::json(nullptr))}, {"sfxxpj", entry.value("sfxxpj", nlohmann::json("1"))}, {"sqzt", nullptr}, {"yxfz", nullptr}, {"zsxz", entry.value("pjrjsdm", nlohmann::json(""))}, {"sfnm", "1"}});
        }
        if (pjjglist.empty()) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", "问卷没有可提交对象"}});
            continue;
        }
        auto submit = request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/submitSaveEvaluation", nlohmann::json{{"pjidlist", nlohmann::json::array()}, {"pjjglist", pjjglist}, {"pjzt", "1"}});
        if (!submit) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", submit.error().message}});
            continue;
        }
        ++success_count;
        results.push_back({{"course", course.title}, {"success", true}, {"message", "评教成功"}});
    }

    Model::MutationResult result;
    result.accepted = success_count > 0;
    result.message = "评教提交完成: " + std::to_string(success_count) + "/" + std::to_string(targets.size());
    result.summary = make_record(target_id.empty() ? "evaluation-submit" : target_id, "自动评教", success_count == static_cast<int>(targets.size()) ? "submitted" : "partial", {{"results", results.dump()}});
    return result;
}

} // namespace UBAANext
