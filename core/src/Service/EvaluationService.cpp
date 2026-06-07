#include <UBAANext/Service/EvaluationService.hpp>

#include <UBAANext/Net/HttpHeaders.hpp>
#include <UBAANext/Net/VpnCipher.hpp>
#include <UBAANext/Parser/EvaluationParser.hpp>
#include <UBAANext/Protocol/DownstreamSessionTypes.hpp>
#include <UBAANext/Protocol/RedirectNavigator.hpp>
#include <UBAANext/Protocol/SessionGuards.hpp>
#include <UBAANext/Service/ResponseUtils.hpp>

#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <utility>

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
    request.headers["User-Agent"] = kUserAgent;
    request.headers["X-Requested-With"] = "XMLHttpRequest";
}

std::string header_value(const HttpResponse &response, const std::string &name) {
    auto value = Protocol::header_value(response, name);
    auto newline = value.find('\n');
    return newline == std::string::npos ? value : value.substr(0, newline);
}

std::string resolve_redirect_url(const std::string &base_url, const std::string &location) {
    return Protocol::resolve_location(base_url, location);
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
        {"pjrdm", task.evaluator_code},
        {"pjrmc", task.evaluator_name},
        {"rwh", task.assignment_no},
        {"xn", task.year},
        {"xq", task.semester},
        {"pjlxid", task.evaluation_type_id},
        {"sfksqbpj", task.allow_all},
        {"yxsfktjst", task.department_submit_status},
        {"ypjcs", std::to_string(task.evaluated_count)},
        {"xypjcs", std::to_string(task.required_count)},
    });
}

bool matches_task(const Model::EvaluationTask &task, const std::string &target_id) {
    if (target_id.empty()) return true;
    return task.id == target_id || task.course_code == target_id || task.task_id == target_id;
}

std::string evaluation_topic_url(const Model::EvaluationTask &task) {
    return "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getQuestionnaireTopic?id=&rwid=" + url_encode(task.task_id) +
           "&wjid=" + url_encode(task.questionnaire_id) +
           "&zdmc=STID&ypjcs=" + std::to_string(task.evaluated_count) +
           "&xypjcs=" + std::to_string(task.required_count) +
           "&sxz=&pjrdm=" + url_encode(task.evaluator_code) +
           "&pjrmc=" + url_encode(task.evaluator_name) +
           "&bpdm=" + url_encode(task.teacher_code) +
           "&bpmc=" + url_encode(task.teacher) +
           "&kcdm=" + url_encode(task.course_code) +
           "&kcmc=" + url_encode(task.title) +
           "&rwh=" + url_encode(task.assignment_no) +
           "&xn=" + url_encode(task.year) +
           "&xq=" + url_encode(task.semester) +
           "&xnxq=" + url_encode(task.term_code) +
           "&pjlxid=" + url_encode(task.evaluation_type_id) +
           "&sfksqbpj=" + url_encode(task.allow_all) +
           "&yxsfktjst=" + url_encode(task.department_submit_status) +
           "&yxdm=";
}

Model::FeatureRecord form_to_record(const Model::EvaluationForm &form) {
    int choice_count = 0;
    for (const auto &question : form.questions) choice_count += static_cast<int>(question.choices.size());
    return make_record(form.task.id, form.task.title, form.task.status, {
        {"teacher", form.task.teacher},
        {"courseCode", form.task.course_code},
        {"rwid", form.task.task_id},
        {"wjid", form.task.questionnaire_id},
        {"xnxq", form.task.term_code},
        {"msid", form.task.pattern_id},
        {"formResultId", form.form_result_id},
        {"relationId", form.evaluated_relation_id},
        {"questionCount", std::to_string(form.questions.size())},
        {"choiceCount", std::to_string(choice_count)},
    });
}

std::string field_or(const Model::EvaluationForm &form, const std::string &key, const std::string &fallback = {}) {
    const auto it = form.submit_fields.find(key);
    if (it != form.submit_fields.end() && !it->second.empty()) return it->second;
    return fallback;
}

nlohmann::json json_string_or_null(const std::string &value) {
    if (value.empty()) return nullptr;
    return value;
}

nlohmann::json field_or_null(const Model::EvaluationForm &form, const std::string &key, const std::string &fallback = {}) {
    return json_string_or_null(field_or(form, key, fallback));
}

nlohmann::json result_map_json(const Model::EvaluationForm &form) {
    nlohmann::json map = nlohmann::json::object();
    for (const auto &[key, value] : form.result_map) map[key] = value;
    if (!map.empty()) return map;
    if (!form.result_detail_map_id.empty()) map["PJJGXXBM"] = form.result_detail_map_id;
    if (!form.task.task_id.empty()) map["RWID"] = form.task.task_id;
    if (!form.result_map_id.empty()) map["PJJGBM"] = form.result_map_id;
    return map.empty() ? nlohmann::json(nullptr) : map;
}

const Model::EvaluationAnswer *find_answer(const std::vector<Model::EvaluationAnswer> &answers, const std::string &question_id) {
    for (const auto &answer : answers) {
        if (answer.question_id == question_id) return &answer;
    }
    return nullptr;
}

std::string html_paragraph(const std::string &text) {
    return text.empty() ? std::string{} : "<p>" + text + "</p>";
}

Result<nlohmann::json> build_submission_body(const Model::EvaluationForm &form,
                                             const std::vector<Model::EvaluationAnswer> &answers,
                                             const std::string &reason) {
    if (form.questions.empty()) return make_error(ErrorCode::ParseError, "问卷没有题目");
    const auto relation_id = field_or(form, "wjssrwid", form.evaluated_relation_id);
    if (relation_id.empty()) return make_error(ErrorCode::ParseError, "问卷缺少 wjssrwid");

    nlohmann::json pjxxlist = nlohmann::json::array();
    double score = 0.0;
    bool score_seen = false;
    bool first_choice = true;
    for (const auto &question : form.questions) {
        const auto *answer = find_answer(answers, question.id);
        nlohmann::json answer_ids = nlohmann::json::array();
        std::string completion_option_id;
        if (!question.choices.empty()) completion_option_id = question.choices.front().id;

        if (question.is_choice) {
            const Model::EvaluationChoice *selected = nullptr;
            if (answer != nullptr && !answer->choice_id.empty()) {
                for (const auto &choice : question.choices) {
                    if (choice.id == answer->choice_id) {
                        selected = &choice;
                        break;
                    }
                }
                if (selected == nullptr) return make_error(ErrorCode::InvalidArgument, "评教答案引用了不存在的选项: " + answer->choice_id);
            } else if (!question.choices.empty()) {
                const auto index = first_choice && question.choices.size() > 1 ? std::size_t{1} : std::size_t{0};
                selected = &question.choices[index];
            }
            first_choice = false;
            if (selected != nullptr && !selected->id.empty()) {
                answer_ids.push_back(selected->id);
                score += selected->score;
                score_seen = score_seen || selected->score > 0.0;
            }
        } else if (answer != nullptr && !answer->text.empty()) {
            answer_ids.push_back(html_paragraph(answer->text));
        }

        nlohmann::json question_body = {
            {"sjly", "1"},
            {"stlx", question.type.empty() ? "1" : question.type},
            {"wjid", form.task.questionnaire_id},
            {"wjssrwid", relation_id},
            {"wjstctid", question.is_choice ? std::string{} : completion_option_id},
            {"wjstid", question.id},
            {"xxdalist", answer_ids},
        };
        if (question.is_choice) question_body["wtjjy"] = "";
        pjxxlist.push_back(std::move(question_body));
    }

    const auto final_score = score_seen ? score : 93.0;
    if ((final_score >= 100.0 || final_score < 60.0) && reason.empty()) {
        return make_error(ErrorCode::InvalidArgument, "满分或不及格评教提交需要提供 10-200 字原因");
    }
    if (!reason.empty() && (reason.size() < 10 || reason.size() > 200)) {
        return make_error(ErrorCode::InvalidArgument, "评教原因长度必须在 10-200 字之间");
    }

    nlohmann::json pjjglist = nlohmann::json::array();
    pjjglist.push_back({
        {"bprdm", field_or_null(form, "bprdm", form.task.teacher_code)},
        {"bprmc", field_or_null(form, "bprmc", form.task.teacher)},
        {"kcdm", field_or_null(form, "kcdm", form.task.course_code)},
        {"kcmc", field_or_null(form, "kcmc", form.task.title)},
        {"pjdf", final_score},
        {"pjfs", field_or(form, "pjfs", "1")},
        {"pjid", field_or_null(form, "pjid", form.form_result_id)},
        {"pjlx", field_or_null(form, "pjlx", form.task.evaluation_type_id)},
        {"pjmap", result_map_json(form)},
        {"pjrdm", field_or_null(form, "pjrdm", form.task.evaluator_code)},
        {"pjrjsdm", field_or_null(form, "pjrjsdm", form.evaluator_role_id)},
        {"pjrxm", field_or_null(form, "pjrxm", form.task.evaluator_name)},
        {"pjsx", 1},
        {"pjxxlist", pjxxlist},
        {"rwh", field_or_null(form, "rwh", form.task.assignment_no)},
        {"stzjid", "xx"},
        {"wjid", form.task.questionnaire_id},
        {"wjssrwid", relation_id},
        {"wtjjy", reason},
        {"xhgs", nullptr},
        {"xnxq", field_or_null(form, "xnxq", form.task.term_code)},
        {"sfxxpj", field_or(form, "sfxxpj", "1")},
        {"sqzt", nullptr},
        {"yxfz", nullptr},
        {"zsxz", field_or(form, "pjrjsdm", form.evaluator_role_id)},
        {"sfnm", "1"},
    });
    return nlohmann::json{{"pjidlist", nlohmann::json::array()}, {"pjjglist", pjjglist}, {"pjzt", "1"}};
}

} // namespace

EvaluationService::EvaluationService(IHttpClient &http_client, ICacheStore &cache, ConnectionMode mode)
    : m_http_client(http_client), m_cache(cache), m_mode(mode) {}

namespace {

Result<HttpResponse> send_evaluation_request(IHttpClient &http_client,
                                             ConnectionMode mode,
                                             HttpMethod method,
                                             const std::string &url,
                                             const nlohmann::json &body,
                                             const char *failure_message) {
    std::string current_url = url;
    for (int redirects = 0; redirects < 8; ++redirects) {
        HttpRequest request;
        request.method = method;
        request.url = resolve_for_mode(current_url, mode);
        apply_headers(request);
        if (method == HttpMethod::Post) {
            request.headers["Content-Type"] = "application/json";
            if (!body.is_null()) request.body = body.dump();
        }

        auto response = http_client.send(request);
        if (!response) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::Unavailable,
                                                         std::string(failure_message) + ": " + response.error().message,
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        if (response->status_code < 300 || response->status_code >= 400) return *response;

        auto location = header_value(*response, "Location");
        if (location.empty()) {
            auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                         Protocol::DownstreamActivationStage::RedirectFollow,
                                                         Protocol::DownstreamSessionState::ProtocolError,
                                                         "评教跳转缺少 Location",
                                                         Protocol::redact_url_query(current_url));
            return make_error(error.code, Protocol::to_error(error).message);
        }
        current_url = resolve_redirect_url(current_url, location);
        method = HttpMethod::Get;
    }
    auto error = Protocol::make_downstream_error(Protocol::DownstreamSystemId::Spoc,
                                                 Protocol::DownstreamActivationStage::RedirectFollow,
                                                 Protocol::DownstreamSessionState::Unavailable,
                                                 "评教跳转次数过多",
                                                 Protocol::redact_url_query(current_url));
    return make_error(error.code, Protocol::to_error(error).message);
}

} // namespace

Result<void> EvaluationService::activate_session() {
    (void)m_cache;
    if (m_activated) return {};
    auto response = send_evaluation_request(m_http_client, m_mode, HttpMethod::Get, "https://spoc.buaa.edu.cn/pjxt/cas", nlohmann::json(nullptr), "激活评教会话失败");
    if (!response) return make_error(response.error().code, response.error().message);
    if (ServiceResponse::is_session_expired_response(*response)) return make_error(ErrorCode::SessionExpired, "评教会话已过期，请重新登录");
    if (response->status_code < 200 || response->status_code >= 300) return make_error(ErrorCode::NetworkError, "评教会话激活返回: " + std::to_string(response->status_code));
    m_activated = true;
    return {};
}

Result<nlohmann::json> EvaluationService::request_json(HttpMethod method, const std::string &url, const nlohmann::json &body) {
    auto response = send_evaluation_request(m_http_client, m_mode, method, url, body, "请求评教失败");
    if (!response) return make_error(response.error().code, response.error().message);
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

Result<Model::EvaluationForm> EvaluationService::request_form(const Model::EvaluationTask &task) {
    if (task.task_id.empty() || task.questionnaire_id.empty()) return make_error(ErrorCode::InvalidArgument, "评教任务缺少 rwid/wjid");
    (void)request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern",
                       nlohmann::json{{"rwid", task.task_id}, {"wjid", task.questionnaire_id}, {"msid", task.pattern_id.empty() ? "1" : task.pattern_id}});
    auto topic_data = request_json(HttpMethod::Get, evaluation_topic_url(task));
    if (!topic_data) return make_error(topic_data.error().code, topic_data.error().message);
    auto form = Parser::parse_evaluation_form(*topic_data, task);
    if (form.questions.empty()) return make_error(ErrorCode::ParseError, "问卷没有题目");
    return form;
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
        if (!questionnaires_data) return make_error(questionnaires_data.error().code, questionnaires_data.error().message);
        if (!questionnaires_data->is_array()) return make_error(ErrorCode::ParseError, "评教问卷响应不是数组");
        for (const auto &questionnaire : *questionnaires_data) {
            auto wjid = json_string(questionnaire, "wjid");
            auto msid = json_string(questionnaire, "msid");
            if (msid.empty()) msid = "1";
            if (wjid.empty()) continue;
            (void)request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/reviseQuestionnairePattern", nlohmann::json{{"rwid", rwid}, {"wjid", wjid}, {"msid", msid}});
            for (const auto &sfyp : {std::string("0"), std::string("1")}) {
                auto courses_data = request_json(HttpMethod::Get, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/getRequiredReviewsData?sfyp=" + sfyp + "&wjid=" + url_encode(wjid) + "&xnxq=" + url_encode(*xnxq) + "&pageNum=1&pageSize=999");
                if (!courses_data) return make_error(courses_data.error().code, courses_data.error().message);
                if (!courses_data->is_array()) return make_error(ErrorCode::ParseError, "评教课程响应不是数组");
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

Result<Model::EvaluationForm> EvaluationService::get_form(const std::string &target_id) {
    if (target_id.empty()) return make_error(ErrorCode::InvalidArgument, "evaluation form 需要 --id 或课程代码");
    auto tasks = list_evaluation_tasks();
    if (!tasks) return make_error(tasks.error().code, tasks.error().message);
    for (const auto &task : *tasks) {
        if (task.status != "pending" || !matches_task(task, target_id)) continue;
        return request_form(task);
    }
    return make_error(ErrorCode::InvalidArgument, "未找到指定待评教课程: " + target_id);
}

Result<Model::FeatureRecord> EvaluationService::form_record(const std::string &target_id) {
    auto form = get_form(target_id);
    if (!form) return make_error(form.error().code, form.error().message);
    return form_to_record(*form);
}

void EvaluationService::set_write_operation_gate(WriteOperationGate gate) {
    m_write_gate = std::move(gate);
}

Result<Model::MutationResult> EvaluationService::submit_evaluations(const std::string &target_id) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    auto tasks = list_evaluation_tasks();
    if (!tasks) return make_error(tasks.error().code, tasks.error().message);

    std::vector<Model::EvaluationTask> targets;
    for (const auto &task : *tasks) {
        if (task.status != "pending" || !matches_task(task, target_id)) continue;
        targets.push_back(task);
    }
    if (targets.empty()) return make_error(ErrorCode::InvalidArgument, target_id.empty() ? "没有待提交的评教课程" : "未找到指定待评教课程: " + target_id);

    nlohmann::json results = nlohmann::json::array();
    int success_count = 0;
    for (const auto &course : targets) {
        auto form = request_form(course);
        if (!form) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", form.error().message}});
            continue;
        }
        auto body = build_submission_body(*form, {}, {});
        if (!body) {
            results.push_back({{"course", course.title}, {"success", false}, {"message", body.error().message}});
            continue;
        }
        auto submit = request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/submitSaveEvaluation", *body);
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

Result<Model::MutationResult> EvaluationService::submit_form(const Model::EvaluationSubmission &submission) {
    auto allowed = require_write_operation(m_write_gate);
    if (!allowed) return make_error(allowed.error().code, allowed.error().message);
    if (submission.target_id.empty()) return make_error(ErrorCode::InvalidArgument, "evaluation form submit 需要 --id 或课程代码");
    auto form = get_form(submission.target_id);
    if (!form) return make_error(form.error().code, form.error().message);
    auto body = build_submission_body(*form, submission.answers, submission.reason);
    if (!body) return make_error(body.error().code, body.error().message);
    auto submit = request_json(HttpMethod::Post, "https://spoc.buaa.edu.cn/pjxt/evaluationMethodSix/submitSaveEvaluation", *body);
    if (!submit) return make_error(submit.error().code, submit.error().message);

    Model::MutationResult result;
    result.accepted = true;
    result.message = "评教表单提交成功";
    result.summary = make_record(form->task.id, form->task.title, "submitted", {
        {"courseCode", form->task.course_code},
        {"teacher", form->task.teacher},
        {"questionCount", std::to_string(form->questions.size())},
    });
    return result;
}

} // namespace UBAANext
