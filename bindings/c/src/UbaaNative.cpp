#include <UBAANext/Bindings/C/UbaaNative.h>

#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Model/Signin.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>
#include <UBAANext/Model/Ygdk.hpp>
#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>
#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>
#include <UBAANext/Platform/PlatformCapabilities.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/TermService.hpp>
#include <UBAANext/Service/TodoService.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Service/YgdkService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>
#include <UBAANext/Version.hpp>

#if defined(_WIN32)
#    include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>
#elif defined(__OHOS__)
#    include <UBAANext/Platform/Harmony/HarmonyPlatformCapabilities.hpp>
#elif defined(__linux__)
#    include <UBAANext/Platform/Linux/LinuxPlatformCapabilities.hpp>
#endif

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using json = nlohmann::json;

[[nodiscard]] UBAANext::PlatformCapabilities current_capabilities() {
#if defined(_WIN32)
    return UBAANext::Platform::Windows::WindowsPlatformCapabilities{}.capabilities();
#elif defined(__OHOS__)
    return UBAANext::Platform::Harmony::HarmonyPlatformCapabilities{}.capabilities();
#elif defined(__linux__)
    return UBAANext::Platform::Linux::LinuxPlatformCapabilities{}.capabilities();
#else
    return UBAANext::PlatformCapabilities{};
#endif
}

[[nodiscard]] uint8_t as_u8(bool value) {
    return value ? 1U : 0U;
}

void write_capabilities(const UBAANext::PlatformCapabilities &caps, UbaaNextCapabilities &out_capabilities) {
    std::memset(&out_capabilities, 0, sizeof(out_capabilities));
    out_capabilities.real_network = as_u8(caps.real_network);
    out_capabilities.secure_cookie_persistence = as_u8(caps.secure_cookie_persistence);
    out_capabilities.cookie_persistence = as_u8(caps.cookie_persistence);
    out_capabilities.redirect_control = as_u8(caps.redirect_control);
    out_capabilities.openssl_crypto = as_u8(caps.openssl_crypto);
    out_capabilities.secure_store = as_u8(caps.secure_store);
    out_capabilities.app_data_path = as_u8(caps.app_data_path);
    out_capabilities.upload_bytes = as_u8(caps.upload_bytes);
    out_capabilities.live_login = as_u8(caps.live_login);
    out_capabilities.write_operations = as_u8(caps.write_operations);
}

class VolatileSecureStore final : public UBAANext::ISecureStore {
public:
    void set_string(const std::string &key, const std::string &value) override {
        m_values[key] = value;
    }

    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override {
        auto it = m_values.find(key);
        if (it == m_values.end()) return std::nullopt;
        return it->second;
    }

    void remove(const std::string &key) override {
        m_values.erase(key);
    }

    void clear() override {
        m_values.clear();
    }

private:
    std::unordered_map<std::string, std::string> m_values;
};

struct RuntimeBucket {
    VolatileSecureStore store;
    UBAANext::MemoryCacheStore cache;
    UBAANext::Platform::Curl::CurlNetworkStack network{store};
};

} // namespace

struct UbaaNextContext {
    UBAANext::PlatformCapabilities capabilities = current_capabilities();
    UBAANext::ConnectionMode mode = UBAANext::ConnectionMode::WebVPN;
    RuntimeBucket direct;
    RuntimeBucket webvpn;
#if UBAANEXT_ENABLE_MOCKS
    RuntimeBucket mock;
#endif
    std::mutex mutex;

    RuntimeBucket &bucket() {
#if UBAANEXT_ENABLE_MOCKS
        if (mode == UBAANext::ConnectionMode::Mock) return mock;
#endif
        return mode == UBAANext::ConnectionMode::Direct ? direct : webvpn;
    }
};

namespace {

[[nodiscard]] const char *copy_result(const std::string &payload) {
    auto *buffer = new (std::nothrow) char[payload.size() + 1];
    if (!buffer) return nullptr;
    std::memcpy(buffer, payload.c_str(), payload.size() + 1);
    return buffer;
}

[[nodiscard]] const char *json_result(const json &payload) {
    return copy_result(payload.dump());
}

[[nodiscard]] json success(json data) {
    return json{{"ok", true}, {"data", std::move(data)}, {"error", nullptr}};
}

[[nodiscard]] json failure(UBAANext::ErrorCode code, std::string message) {
    return json{{"ok", false},
                {"data", nullptr},
                {"error", {{"code", std::string(UBAANext::error_code_to_string(code))},
                            {"message", UBAANext::Security::redact_sensitive_text(message)}}}};
}

[[nodiscard]] json failure(const UBAANext::Error &error) {
    return failure(error.code, error.message);
}

[[nodiscard]] const char *error_result(UBAANext::ErrorCode code, std::string message) {
    return json_result(failure(code, std::move(message)));
}

[[nodiscard]] const char *null_context_result() {
    return error_result(UBAANext::ErrorCode::InvalidArgument, "C ABI context 为空");
}

[[nodiscard]] std::string c_string_or_empty(const char *value) {
    return value == nullptr ? std::string{} : std::string{value};
}

[[nodiscard]] UBAANext::Result<UBAANext::ConnectionMode> parse_connection_mode(const char *mode) {
    const auto value = c_string_or_empty(mode);
#if UBAANEXT_ENABLE_MOCKS
    if (value == "mock") return UBAANext::ConnectionMode::Mock;
#endif
    if (value == "direct") return UBAANext::ConnectionMode::Direct;
    if (value == "vpn" || value == "webvpn") return UBAANext::ConnectionMode::WebVPN;
    return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument, "连接模式必须是 direct、vpn 或 webvpn");
}

[[nodiscard]] const char *mode_to_string(UBAANext::ConnectionMode mode) {
#if UBAANEXT_ENABLE_MOCKS
    if (mode == UBAANext::ConnectionMode::Mock) return "mock";
#endif
    return mode == UBAANext::ConnectionMode::Direct ? "direct" : "webvpn";
}

[[nodiscard]] UBAANext::AuthService create_auth_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    UBAANext::AuthService auth(bucket.network.http_client(), bucket.store);
    auth.set_connection_mode(context.mode);
    return auth;
}

[[nodiscard]] UBAANext::CourseService create_course_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::CourseService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] UBAANext::GradeService create_grade_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::GradeService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] UBAANext::ExamService create_exam_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::ExamService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] UBAANext::TodoService create_todo_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::TodoService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] UBAANext::TermService create_term_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::TermService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] UBAANext::SigninService create_signin_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    auto auth = create_auth_service(context);
    auto account = auth.restore_session();
    std::string student_id = account ? account->student_id : std::string{};
    return UBAANext::SigninService(bucket.network.http_client(), bucket.cache, context.mode, std::move(student_id));
}

[[nodiscard]] UBAANext::YgdkService create_ygdk_service(UbaaNextContext &context) {
    auto &bucket = context.bucket();
    return UBAANext::YgdkService(bucket.network.http_client(), bucket.cache, context.mode);
}

[[nodiscard]] json account_to_json(const UBAANext::Model::Account &account) {
    return json{{"studentId", account.student_id}, {"displayName", account.display_name}};
}

[[nodiscard]] json course_to_json(const UBAANext::Model::Course &course) {
    return json{{"id", course.id},
                {"name", course.name},
                {"teacher", course.teacher},
                {"classroom", course.classroom},
                {"weekStart", course.week_start},
                {"weekEnd", course.week_end},
                {"dayOfWeek", course.day_of_week},
                {"sectionStart", course.section_start},
                {"sectionEnd", course.section_end},
                {"courseCode", course.course_code},
                {"credit", course.credit},
                {"beginTime", course.begin_time},
                {"endTime", course.end_time}};
}

[[nodiscard]] json grade_to_json(const UBAANext::Model::Grade &grade) {
    return json{{"id", grade.id},
                {"courseName", grade.course_name},
                {"courseCode", grade.course_code},
                {"courseType", grade.course_type},
                {"credit", grade.credit},
                {"score", grade.score},
                {"gradePoint", grade.grade_point},
                {"termCode", grade.term_code},
                {"status", grade.raw_status}};
}

[[nodiscard]] json exam_to_json(const UBAANext::Model::Exam &exam) {
    return json{{"id", exam.id},
                {"courseName", exam.course_name},
                {"location", exam.location},
                {"timeText", exam.time_text},
                {"courseNo", exam.course_no},
                {"examDate", exam.exam_date},
                {"startTime", exam.start_time},
                {"endTime", exam.end_time},
                {"seatNo", exam.seat_no},
                {"examType", exam.exam_type},
                {"status", static_cast<int>(exam.status)}};
}

[[nodiscard]] json term_to_json(const UBAANext::Model::Term &term) {
    return json{{"code", term.code}, {"name", term.name}, {"selected", term.selected}, {"index", term.index}};
}

[[nodiscard]] json week_to_json(const UBAANext::Model::Week &week) {
    return json{{"serialNumber", week.serial_number},
                {"name", week.name},
                {"startDate", week.start_date},
                {"endDate", week.end_date},
                {"isCurrent", week.is_current}};
}

[[nodiscard]] json feature_record_to_json(const UBAANext::Model::FeatureRecord &record) {
    return json{{"id", record.id}, {"title", record.title}, {"status", record.status}, {"fields", record.fields}};
}

[[nodiscard]] json mutation_result_to_json(const UBAANext::Model::MutationResult &result) {
    return json{{"accepted", result.accepted}, {"message", result.message}, {"result", feature_record_to_json(result.summary)}};
}

[[nodiscard]] json signin_course_to_json(const UBAANext::Model::SigninCourse &course) {
    return json{{"id", course.id},
                {"name", course.name},
                {"status", course.status},
                {"classBeginTime", course.class_begin_time},
                {"classEndTime", course.class_end_time},
                {"signStatus", course.sign_status}};
}

[[nodiscard]] json ygdk_item_to_json(const UBAANext::Model::YgdkItem &item) {
    return json{{"id", item.id}, {"name", item.name}, {"classifyId", item.classify_id}, {"sort", item.sort}};
}

[[nodiscard]] json ygdk_overview_to_json(const UBAANext::Model::YgdkOverview &overview) {
    return json{{"classify", {{"id", overview.classify.id}, {"name", overview.classify.name}}},
                {"termName", overview.term_name},
                {"termCount", overview.term_count},
                {"termGoodCount", overview.term_good_count},
                {"weekCount", overview.week_count},
                {"monthCount", overview.month_count},
                {"dayCount", overview.day_count}};
}

[[nodiscard]] json ygdk_record_to_json(const UBAANext::Model::YgdkRecord &record) {
    return json{{"id", record.id},
                {"itemName", record.item_name},
                {"state", record.state},
                {"place", record.place},
                {"startTime", record.start_time},
                {"endTime", record.end_time},
                {"createdAt", record.created_at}};
}

template <typename T, typename Serializer>
[[nodiscard]] json array_json(const std::vector<T> &items, Serializer serializer) {
    json arr = json::array();
    for (const auto &item : items) {
        arr.push_back(serializer(item));
    }
    return arr;
}

template <typename Result, typename Serializer>
[[nodiscard]] const char *return_service_result(Result &&result, Serializer serializer) {
    if (!result) return json_result(failure(result.error()));
    return json_result(success(serializer(*result)));
}

template <typename Callback>
[[nodiscard]] const char *with_context(UbaaNextContext *context, Callback callback) {
    if (!context) return null_context_result();
    try {
        std::lock_guard<std::mutex> lock(context->mutex);
        return callback(*context);
    } catch (const UBAANext::ResultError &error) {
        return json_result(failure(error.error()));
    } catch (const std::exception &error) {
        return error_result(UBAANext::ErrorCode::Unknown, error.what());
    } catch (...) {
        return error_result(UBAANext::ErrorCode::Unknown, "未知 C ABI 异常");
    }
}

} // namespace

extern "C" {

const char *ubaanext_version(void) {
    return UBAANEXT_VERSION_STRING;
}

int32_t ubaanext_get_capabilities(UbaaNextCapabilities *out_capabilities) {
    if(out_capabilities == nullptr) {
        return -1;
    }

    write_capabilities(current_capabilities(), *out_capabilities);
    return 0;
}

UbaaNextContext *ubaanext_context_create(void) {
    try {
        UBAANext::Platform::OpenSSL::install_open_ssl_crypto_provider();
        return new (std::nothrow) UbaaNextContext();
    } catch (...) {
        return nullptr;
    }
}

void ubaanext_context_release(UbaaNextContext *context) {
    delete context;
}

int32_t ubaanext_context_set_connection_mode(UbaaNextContext *context, const char *mode) {
    if (!context) return -1;
    auto parsed = parse_connection_mode(mode);
    if (!parsed) return -2;
    std::lock_guard<std::mutex> lock(context->mutex);
    context->mode = *parsed;
    return 0;
}

void ubaanext_release_result(const char *result_json) {
    delete[] result_json;
}

const char *ubaanext_auth_login(UbaaNextContext *context, const char *username, const char *password, const char *captcha) {
    return with_context(context, [username, password, captcha](UbaaNextContext &ctx) {
        if (!username || !password) {
            return error_result(UBAANext::ErrorCode::InvalidArgument, "用户名和密码不能为空");
        }
        if (!ctx.capabilities.live_login) {
            return error_result(UBAANext::ErrorCode::UnsupportedPlatform, "当前平台未启用真实登录能力");
        }
        auto auth = create_auth_service(ctx);
        return return_service_result(auth.login_real(c_string_or_empty(username), c_string_or_empty(password), ctx.mode, c_string_or_empty(captcha)),
                                     [](const UBAANext::Model::Account &account) { return json{{"account", account_to_json(account)}}; });
    });
}

const char *ubaanext_auth_logout(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto auth = create_auth_service(ctx);
        auto result = auth.logout();
        if (!result) return json_result(failure(result.error()));
        return json_result(success(json{{"active", false}}));
    });
}

const char *ubaanext_auth_restore_session(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto auth = create_auth_service(ctx);
        return return_service_result(auth.restore_session(), [](const UBAANext::Model::Account &account) {
            return json{{"active", true}, {"account", account_to_json(account)}};
        });
    });
}

const char *ubaanext_auth_get_session_state(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto auth = create_auth_service(ctx);
        auto restored = auth.restore_session();
        const auto active = static_cast<bool>(restored);
        json data{{"active", active}, {"mode", mode_to_string(ctx.mode)}};
        data["account"] = active ? account_to_json(*restored) : json(nullptr);
        return json_result(success(std::move(data)));
    });
}

const char *ubaanext_terms(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto service = create_term_service(ctx);
        return return_service_result(service.get_terms(), [](const std::vector<UBAANext::Model::Term> &terms) {
            return json{{"terms", array_json(terms, term_to_json)}};
        });
    });
}

const char *ubaanext_weeks(UbaaNextContext *context, const char *term_code) {
    return with_context(context, [term_code](UbaaNextContext &ctx) {
        auto service = create_term_service(ctx);
        return return_service_result(service.get_weeks(c_string_or_empty(term_code)), [](const std::vector<UBAANext::Model::Week> &weeks) {
            return json{{"weeks", array_json(weeks, week_to_json)}};
        });
    });
}

const char *ubaanext_courses_today(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto service = create_course_service(ctx);
        return return_service_result(service.get_today_courses(), [](const std::vector<UBAANext::Model::Course> &courses) {
            return json{{"courses", array_json(courses, course_to_json)}};
        });
    });
}

const char *ubaanext_courses_date(UbaaNextContext *context, const char *date) {
    return with_context(context, [date](UbaaNextContext &ctx) {
        auto service = create_course_service(ctx);
        return return_service_result(service.get_date_courses(c_string_or_empty(date)), [](const std::vector<UBAANext::Model::Course> &courses) {
            return json{{"courses", array_json(courses, course_to_json)}};
        });
    });
}

const char *ubaanext_courses_week(UbaaNextContext *context, int32_t week, const char *term_code) {
    return with_context(context, [week, term_code](UbaaNextContext &ctx) {
        auto service = create_course_service(ctx);
        const auto term = c_string_or_empty(term_code);
        auto result = term.empty() ? service.get_week_courses(static_cast<int>(week)) : service.get_week_courses(static_cast<int>(week), term);
        return return_service_result(std::move(result), [](const std::vector<UBAANext::Model::Course> &courses) {
            return json{{"courses", array_json(courses, course_to_json)}};
        });
    });
}

const char *ubaanext_grades(UbaaNextContext *context, const char *term_code) {
    return with_context(context, [term_code](UbaaNextContext &ctx) {
        auto service = create_grade_service(ctx);
        const auto term = c_string_or_empty(term_code);
        auto result = term.empty() ? service.list_all_grades() : service.list_grades(term);
        return return_service_result(std::move(result), [](const std::vector<UBAANext::Model::Grade> &grades) {
            return json{{"grades", array_json(grades, grade_to_json)}};
        });
    });
}

const char *ubaanext_exams(UbaaNextContext *context, const char *term_code) {
    return with_context(context, [term_code](UbaaNextContext &ctx) {
        auto service = create_exam_service(ctx);
        return return_service_result(service.get_exams(c_string_or_empty(term_code)), [](const std::vector<UBAANext::Model::Exam> &exams) {
            return json{{"exams", array_json(exams, exam_to_json)}};
        });
    });
}

const char *ubaanext_todos(UbaaNextContext *context, uint8_t pending_only) {
    return with_context(context, [pending_only](UbaaNextContext &ctx) {
        auto service = create_todo_service(ctx);
        UBAANext::TodoQuery query;
        query.pending_only = pending_only != 0;
        return return_service_result(service.list_todos(query), [](const std::vector<UBAANext::Model::FeatureRecord> &todos) {
            return json{{"todos", array_json(todos, feature_record_to_json)}};
        });
    });
}

const char *ubaanext_signin_today(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto service = create_signin_service(ctx);
        return return_service_result(service.list_today_courses(), [](const std::vector<UBAANext::Model::SigninCourse> &courses) {
            return json{{"courses", array_json(courses, signin_course_to_json)}};
        });
    });
}

const char *ubaanext_signin_do(UbaaNextContext *context, const char *course_id, uint8_t confirmed) {
    return with_context(context, [course_id, confirmed](UbaaNextContext &ctx) {
        if (!course_id || c_string_or_empty(course_id).empty()) {
            return error_result(UBAANext::ErrorCode::InvalidArgument, "签到课程 ID 不能为空");
        }
        auto service = create_signin_service(ctx);
        service.set_write_operation_gate(UBAANext::confirmed_write_operation(ctx.capabilities, "signin do", confirmed != 0));
        return return_service_result(service.perform_signin(c_string_or_empty(course_id)), [](const UBAANext::Model::MutationResult &result) {
            return json{{"mutation", mutation_result_to_json(result)}};
        });
    });
}

const char *ubaanext_ygdk_overview(UbaaNextContext *context) {
    return with_context(context, [](UbaaNextContext &ctx) {
        auto service = create_ygdk_service(ctx);
        return return_service_result(service.overview_data(), [](const std::pair<UBAANext::Model::YgdkOverview, std::vector<UBAANext::Model::YgdkItem>> &data) {
            return json{{"overview", ygdk_overview_to_json(data.first)}, {"items", array_json(data.second, ygdk_item_to_json)}};
        });
    });
}

const char *ubaanext_ygdk_records(UbaaNextContext *context, int32_t page, int32_t size) {
    return with_context(context, [page, size](UbaaNextContext &ctx) {
        auto service = create_ygdk_service(ctx);
        return return_service_result(service.record_list(static_cast<int>(page), static_cast<int>(size)), [](const std::vector<UBAANext::Model::YgdkRecord> &records) {
            return json{{"records", array_json(records, ygdk_record_to_json)}};
        });
    });
}

}
