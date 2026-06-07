#include <UBAANext/Auth/AuthService.hpp>
#include <UBAANext/Base/Error.hpp>
#include <UBAANext/Bindings/C/UbaaNative.h>
#include <UBAANext/Model/Account.hpp>
#include <UBAANext/Model/Course.hpp>
#include <UBAANext/Model/Exam.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Grade.hpp>
#include <UBAANext/Model/Live.hpp>
#include <UBAANext/Model/Signin.hpp>
#include <UBAANext/Model/Td.hpp>
#include <UBAANext/Model/Term.hpp>
#include <UBAANext/Model/Week.hpp>
#include <UBAANext/Model/Ygdk.hpp>
#include <UBAANext/Platform/Curl/CurlNetworkStack.hpp>
#include <UBAANext/Platform/OpenSSL/OpenSslCryptoInstaller.hpp>
#include <UBAANext/Platform/PlatformCapabilities.hpp>
#include <UBAANext/Security/SecurityRedaction.hpp>
#include <UBAANext/Service/CourseService.hpp>
#include <UBAANext/Service/ExamService.hpp>
#include <UBAANext/Service/FeatureService.hpp>
#include <UBAANext/Service/GradeService.hpp>
#include <UBAANext/Service/LiveService.hpp>
#include <UBAANext/Service/SigninService.hpp>
#include <UBAANext/Service/TermService.hpp>
#include <UBAANext/Service/TodoService.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>
#include <UBAANext/Service/YgdkService.hpp>
#include <UBAANext/Storage/MemoryCacheStore.hpp>
#include <UBAANext/Storage/SecureStore.hpp>
#include <UBAANext/Storage/TdStore.hpp>
#include <UBAANext/Version.hpp>

#if UBAANEXT_ENABLE_MOCKS
#include <UBAANextMocks/MockCacheStore.hpp>
#include <UBAANextMocks/MockHttpClient.hpp>
#endif

#if defined(_WIN32)
#include <UBAANext/Platform/Windows/WindowsAppDataPathProvider.hpp>
#include <UBAANext/Platform/Windows/WindowsPlatformCapabilities.hpp>
#elif defined(__OHOS__)
#include <UBAANext/Platform/Harmony/HarmonyAppDataPathProvider.hpp>
#include <UBAANext/Platform/Harmony/HarmonyPlatformCapabilities.hpp>
#elif defined(__linux__)
#include <UBAANext/Platform/Linux/LinuxAppDataPathProvider.hpp>
#include <UBAANext/Platform/Linux/LinuxPlatformCapabilities.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using json = nlohmann::json;

[[nodiscard]] UBAANext::PlatformCapabilities current_capabilities()
{
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

[[nodiscard]] uint8_t as_u8(bool value)
{
    return value ? 1U : 0U;
}

void write_capabilities(const UBAANext::PlatformCapabilities &caps,
                        UbaaNextCapabilities &out_capabilities)
{
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

[[nodiscard]] json capabilities_to_json(const UBAANext::PlatformCapabilities &caps)
{
    return json{{"realNetwork", caps.real_network},
                {"secureCookiePersistence", caps.secure_cookie_persistence},
                {"cookiePersistence", caps.cookie_persistence},
                {"redirectControl", caps.redirect_control},
                {"opensslCrypto", caps.openssl_crypto},
                {"secureStore", caps.secure_store},
                {"appDataPath", caps.app_data_path},
                {"uploadBytes", caps.upload_bytes},
                {"liveLogin", caps.live_login},
                {"writeOperations", caps.write_operations}};
}

class VolatileSecureStore final : public UBAANext::ISecureStore
{
  public:
    void set_string(const std::string &key, const std::string &value) override
    {
        m_values[key] = value;
    }

    [[nodiscard]] std::optional<std::string> get_string(const std::string &key) const override
    {
        auto it = m_values.find(key);
        if (it == m_values.end())
            return std::nullopt;
        return it->second;
    }

    void remove(const std::string &key) override { m_values.erase(key); }

    void clear() override { m_values.clear(); }

  private:
    std::unordered_map<std::string, std::string> m_values;
};

struct RuntimeBucket
{
    VolatileSecureStore store;
    UBAANext::MemoryCacheStore cache;
    UBAANext::Platform::Curl::CurlNetworkStack network{store};
};

#if UBAANEXT_ENABLE_MOCKS
struct MockRuntimeBucket
{
    VolatileSecureStore store;
    UBAANextMocks::MockCacheStore cache;
    UBAANextMocks::MockHttpClient http;
};
#endif

} // namespace

struct UbaaNextContext
{
    UBAANext::PlatformCapabilities capabilities = current_capabilities();
    UBAANext::ConnectionMode mode = UBAANext::ConnectionMode::WebVPN;
    RuntimeBucket direct;
    RuntimeBucket webvpn;
#if UBAANEXT_ENABLE_MOCKS
    MockRuntimeBucket mock;
#endif
    std::mutex mutex;

    RuntimeBucket &real_bucket()
    {
        return mode == UBAANext::ConnectionMode::Direct ? direct : webvpn;
    }

    [[nodiscard]] UBAANext::Result<UBAANext::TdStore> td_store() const
    {
#if defined(_WIN32)
        UBAANext::Platform::Windows::WindowsAppDataPathProvider provider;
#elif defined(__OHOS__)
        UBAANext::Platform::Harmony::HarmonyAppDataPathProvider provider;
#elif defined(__linux__)
        UBAANext::Platform::Linux::LinuxAppDataPathProvider provider;
#else
        return UBAANext::make_error(UBAANext::ErrorCode::UnsupportedPlatform,
                                    "当前平台未提供 TD AppData 路径");
#endif
        return UBAANext::TdStore::from_app_data_dir(provider);
    }

    UBAANext::IHttpClient &http_client()
    {
#if UBAANEXT_ENABLE_MOCKS
        if (mode == UBAANext::ConnectionMode::Mock)
            return mock.http;
#endif
        return real_bucket().network.http_client();
    }

    UBAANext::ICacheStore &cache_store()
    {
#if UBAANEXT_ENABLE_MOCKS
        if (mode == UBAANext::ConnectionMode::Mock)
            return mock.cache;
#endif
        return real_bucket().cache;
    }

    UBAANext::ICookieStore *cookie_store()
    {
#if UBAANEXT_ENABLE_MOCKS
        if (mode == UBAANext::ConnectionMode::Mock)
            return nullptr;
#endif
        return &real_bucket().network.cookie_store();
    }

    UBAANext::ISecureStore &secure_store()
    {
#if UBAANEXT_ENABLE_MOCKS
        if (mode == UBAANext::ConnectionMode::Mock)
            return mock.store;
#endif
        return real_bucket().store;
    }
};

namespace
{

[[nodiscard]] const char *copy_result(const std::string &payload)
{
    auto *buffer = new (std::nothrow) char[payload.size() + 1];
    if (!buffer)
        return nullptr;
    std::memcpy(buffer, payload.c_str(), payload.size() + 1);
    return buffer;
}

[[nodiscard]] const char *json_result(const json &payload)
{
    return copy_result(payload.dump());
}

[[nodiscard]] json success(json data)
{
    return json{{"ok", true}, {"data", std::move(data)}, {"error", nullptr}};
}

[[nodiscard]] json failure(UBAANext::ErrorCode code, std::string message)
{
    return json{{"ok", false},
                {"data", nullptr},
                {"error",
                 {{"code", std::string(UBAANext::error_code_to_string(code))},
                  {"message", UBAANext::Security::redact_sensitive_text(message)}}}};
}

[[nodiscard]] json failure(const UBAANext::Error &error)
{
    return failure(error.code, error.message);
}

[[nodiscard]] const char *error_result(UBAANext::ErrorCode code, std::string message)
{
    return json_result(failure(code, std::move(message)));
}

[[nodiscard]] const char *null_context_result()
{
    return error_result(UBAANext::ErrorCode::InvalidArgument, "C ABI context 为空");
}

[[nodiscard]] std::string c_string_or_empty(const char *value)
{
    return value == nullptr ? std::string{} : std::string{value};
}

[[nodiscard]] std::string int_text(int value)
{
    return std::to_string(value);
}

[[nodiscard]] std::string optional_int_text(const std::optional<int> &value)
{
    return value ? std::to_string(*value) : std::string{};
}

[[nodiscard]] UBAANext::Result<UBAANext::ConnectionMode> parse_connection_mode(const char *mode)
{
    const auto value = c_string_or_empty(mode);
#if UBAANEXT_ENABLE_MOCKS
    if (value == "mock")
        return UBAANext::ConnectionMode::Mock;
#endif
    if (value == "direct")
        return UBAANext::ConnectionMode::Direct;
    if (value == "vpn" || value == "webvpn")
        return UBAANext::ConnectionMode::WebVPN;
    return UBAANext::make_error(UBAANext::ErrorCode::InvalidArgument,
                                "连接模式必须是 direct、vpn 或 webvpn");
}

[[nodiscard]] const char *mode_to_string(UBAANext::ConnectionMode mode)
{
#if UBAANEXT_ENABLE_MOCKS
    if (mode == UBAANext::ConnectionMode::Mock)
        return "mock";
#endif
    return mode == UBAANext::ConnectionMode::Direct ? "direct" : "webvpn";
}

[[nodiscard]] UBAANext::AuthService create_auth_service(UbaaNextContext &context)
{
    UBAANext::AuthService auth(context.http_client(), context.secure_store());
    auth.set_connection_mode(context.mode);
    return auth;
}

[[nodiscard]] UBAANext::CourseService create_course_service(UbaaNextContext &context)
{
    return UBAANext::CourseService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::GradeService create_grade_service(UbaaNextContext &context)
{
    return UBAANext::GradeService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::ExamService create_exam_service(UbaaNextContext &context)
{
    return UBAANext::ExamService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::FeatureService create_feature_service(UbaaNextContext &context)
{
    return UBAANext::FeatureService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::LiveService create_live_service(UbaaNextContext &context)
{
    return UBAANext::LiveService(context.http_client(), context.cookie_store(),
                                 context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::TodoService create_todo_service(UbaaNextContext &context)
{
    return UBAANext::TodoService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::TermService create_term_service(UbaaNextContext &context)
{
    return UBAANext::TermService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] UBAANext::SigninService create_signin_service(UbaaNextContext &context)
{
    auto auth = create_auth_service(context);
    auto account = auth.restore_session();
    std::string student_id = account ? account->student_id : std::string{};
    return UBAANext::SigninService(context.http_client(), context.cache_store(), context.mode,
                                   std::move(student_id));
}

[[nodiscard]] UBAANext::YgdkService create_ygdk_service(UbaaNextContext &context)
{
    return UBAANext::YgdkService(context.http_client(), context.cache_store(), context.mode);
}

[[nodiscard]] json account_to_json(const UBAANext::Model::Account &account)
{
    return json{{"studentId", account.student_id}, {"displayName", account.display_name}};
}

[[nodiscard]] json course_to_json(const UBAANext::Model::Course &course)
{
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

[[nodiscard]] json grade_to_json(const UBAANext::Model::Grade &grade)
{
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

[[nodiscard]] json exam_to_json(const UBAANext::Model::Exam &exam)
{
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

[[nodiscard]] json term_to_json(const UBAANext::Model::Term &term)
{
    return json{{"code", term.code},
                {"name", term.name},
                {"selected", term.selected},
                {"index", term.index}};
}

[[nodiscard]] json week_to_json(const UBAANext::Model::Week &week)
{
    return json{{"serialNumber", week.serial_number},
                {"name", week.name},
                {"startDate", week.start_date},
                {"endDate", week.end_date},
                {"isCurrent", week.is_current}};
}

[[nodiscard]] json live_schedule_to_json(const UBAANext::Model::LiveSchedule &schedule)
{
    return json{{"courseId", schedule.course_id},
                {"liveId", schedule.live_id},
                {"name", schedule.name},
                {"teacher", schedule.teacher},
                {"rawStatus", schedule.raw_status}};
}

[[nodiscard]] json live_week_to_json(const UBAANext::Model::LiveWeekSchedule &week)
{
    json days = json::array();
    for (const auto &day : week.days)
    {
        json schedules = json::array();
        for (const auto &schedule : day)
            schedules.push_back(live_schedule_to_json(schedule));
        days.push_back(std::move(schedules));
    }
    return json{
        {"startDate", week.start_date}, {"endDate", week.end_date}, {"days", std::move(days)}};
}

[[nodiscard]] json live_video_source_to_json(const UBAANext::Model::LiveVideoSource &source)
{
    return json{{"kind", source.kind}, {"url", source.url}, {"hls", source.hls}};
}

[[nodiscard]] json live_resource_to_json(const UBAANext::Model::LiveResource &resource)
{
    return json{{"id", resource.course_id + ":" + resource.sub_id},
                {"courseId", resource.course_id},
                {"subId", resource.sub_id},
                {"title", resource.title},
                {"courseCode", resource.course_code},
                {"teacher", resource.teacher},
                {"room", resource.room},
                {"subTitle", resource.sub_title},
                {"statusLabel", resource.status_label},
                {"rawStatus", resource.raw_status},
                {"termName", resource.term_name},
                {"courseTime", resource.course_time},
                {"timeSlot", resource.time_slot},
                {"timeRange", resource.time_range},
                {"thumbUrl", resource.thumb_url},
                {"source", resource.source}};
}

[[nodiscard]] json live_detail_to_json(const UBAANext::Model::LiveResourceDetail &detail)
{
    json data = live_resource_to_json(detail);
    data["hasVideo"] = detail.has_video;
    data["primaryVideoUrl"] = detail.primary_video_url;
    data["primaryVideoHls"] = detail.primary_video_hls;
    data["liveUrl"] = detail.live_url;
    data["playbackUrl"] = detail.playback_url;
    data["playbackHls"] = detail.playback_hls;
    data["pptVideoUrl"] = detail.ppt_video_url;
    data["subResourceGuid"] = detail.sub_resource_guid;
    data["pptResourceGuid"] = detail.ppt_resource_guid;
    data["pptGuids"] = detail.ppt_guids;
    json video_sources = json::array();
    for (const auto &source : detail.video_sources)
        video_sources.push_back(live_video_source_to_json(source));
    data["videoSources"] = std::move(video_sources);
    return data;
}

[[nodiscard]] json feature_record_to_json(const UBAANext::Model::FeatureRecord &record)
{
    return json{{"id", record.id},
                {"title", record.title},
                {"status", record.status},
                {"fields", record.fields}};
}

[[nodiscard]] json mutation_result_to_json(const UBAANext::Model::MutationResult &result)
{
    return json{{"accepted", result.accepted},
                {"message", result.message},
                {"result", feature_record_to_json(result.summary)}};
}

[[nodiscard]] json signin_course_to_json(const UBAANext::Model::SigninCourse &course)
{
    return json{{"id", course.id},
                {"name", course.name},
                {"status", course.status},
                {"classBeginTime", course.class_begin_time},
                {"classEndTime", course.class_end_time},
                {"signStatus", course.sign_status}};
}

[[nodiscard]] json ygdk_item_to_json(const UBAANext::Model::YgdkItem &item)
{
    return json{{"id", item.id},
                {"name", item.name},
                {"classifyId", item.classify_id},
                {"sort", item.sort}};
}

[[nodiscard]] json ygdk_overview_to_json(const UBAANext::Model::YgdkOverview &overview)
{
    return json{{"classify", {{"id", overview.classify.id}, {"name", overview.classify.name}}},
                {"termName", overview.term_name},
                {"termCount", overview.term_count},
                {"termGoodCount", overview.term_good_count},
                {"weekCount", overview.week_count},
                {"monthCount", overview.month_count},
                {"dayCount", overview.day_count}};
}

[[nodiscard]] json ygdk_record_to_json(const UBAANext::Model::YgdkRecord &record)
{
    return json{{"id", record.id},
                {"itemName", record.item_name},
                {"state", record.state},
                {"place", record.place},
                {"startTime", record.start_time},
                {"endTime", record.end_time},
                {"createdAt", record.created_at}};
}

[[nodiscard]] json td_user_to_record_json(const UBAANext::Model::Td::User &user)
{
    UBAANext::Model::FeatureRecord record;
    record.id = user.student_id;
    record.title = user.student_id;
    record.status = "configured";
    record.fields["cardId"] = user.card_id;
    record.fields["entranceMachineId"] = int_text(user.entrance_machine_id);
    record.fields["exitMachineId"] = int_text(user.exit_machine_id);
    record.fields["entranceImage"] = user.entrance_image;
    record.fields["exitImage"] = user.exit_image;
    record.fields["rounds"] = int_text(user.rounds);
    record.fields["waitMinMinutes"] = int_text(user.wait_time_min_minutes);
    record.fields["waitMaxMinutes"] = int_text(user.wait_time_max_minutes);
    record.fields["cachedTermCount"] = optional_int_text(user.cached_term_count);
    return feature_record_to_json(record);
}

[[nodiscard]] json td_state_to_record_json(const UBAANext::Model::Td::UserState &state)
{
    UBAANext::Model::FeatureRecord record;
    record.id = state.student_id;
    record.title = state.date.empty() ? state.student_id : state.date;
    record.status = state.status;
    record.fields["studentId"] = state.student_id;
    record.fields["date"] = state.date;
    record.fields["nextAction"] = state.next_action;
    record.fields["completedRounds"] = int_text(state.completed_rounds);
    record.fields["termCount"] = optional_int_text(state.term_count);
    record.fields["nextRunAt"] = state.next_run_at;
    record.fields["lastError"] = UBAANext::Security::redact_sensitive_text(state.last_error);
    record.fields["lastMessage"] = UBAANext::Security::redact_sensitive_text(state.last_message);
    return feature_record_to_json(record);
}

[[nodiscard]] json td_count_record_json(const UBAANext::Model::Td::User &user,
                                        const std::optional<UBAANext::Model::Td::UserState> &state)
{
    UBAANext::Model::FeatureRecord record;
    record.id = user.student_id;
    record.title = user.student_id;
    if (state && state->term_count)
    {
        record.status = "cached-state";
        record.fields["termCount"] = std::to_string(*state->term_count);
        record.fields["source"] = "state";
    }
    else if (user.cached_term_count)
    {
        record.status = "cached-user";
        record.fields["termCount"] = std::to_string(*user.cached_term_count);
        record.fields["source"] = "user";
    }
    else
    {
        record.status = "missing";
        record.fields["termCount"] = "";
        record.fields["source"] = "none";
    }
    record.fields["completedRounds"] = state ? int_text(state->completed_rounds) : std::string{};
    record.fields["lastError"] =
        state ? UBAANext::Security::redact_sensitive_text(state->last_error) : std::string{};
    return feature_record_to_json(record);
}

template <typename T, typename Serializer>
[[nodiscard]] json array_json(const std::vector<T> &items, Serializer serializer)
{
    json arr = json::array();
    for (const auto &item : items)
    {
        arr.push_back(serializer(item));
    }
    return arr;
}

template <typename Result, typename Serializer>
[[nodiscard]] const char *return_service_result(Result &&result, Serializer serializer)
{
    if (!result)
        return json_result(failure(result.error()));
    return json_result(success(serializer(*result)));
}

[[nodiscard]] std::string lowercase_ascii_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

[[nodiscard]] bool live_matches_course_schedule(const UBAANext::Model::LiveResource &resource,
                                                const std::vector<UBAANext::Model::Course> &courses)
{
    const auto title = lowercase_ascii_copy(resource.title);
    const auto code = lowercase_ascii_copy(resource.course_code);
    for (const auto &course : courses)
    {
        const auto course_name = lowercase_ascii_copy(course.name);
        const auto course_code = lowercase_ascii_copy(course.course_code);
        if (!code.empty() && !course_code.empty() && code == course_code)
            return true;
        if (!title.empty() && !course_name.empty() &&
            (title.find(course_name) != std::string::npos ||
             course_name.find(title) != std::string::npos))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] UBAANext::Result<std::vector<UBAANext::Model::LiveResource>> filter_live_from_course(
    UbaaNextContext &context, const std::string &date,
    std::vector<UBAANext::Model::LiveResource> resources)
{
    auto course_service = create_course_service(context);
    auto courses = course_service.get_date_courses(date);
    if (!courses)
        return UBAANext::make_error(courses.error().code,
                                    "按课表过滤课堂资源失败: " + courses.error().message);

    std::vector<UBAANext::Model::LiveResource> filtered;
    for (auto &resource : resources)
    {
        if (live_matches_course_schedule(resource, *courses))
            filtered.push_back(std::move(resource));
    }
    return filtered;
}

template <typename Callback>
[[nodiscard]] const char *with_context(UbaaNextContext *context, Callback callback)
{
    if (!context)
        return null_context_result();
    try
    {
        std::lock_guard<std::mutex> lock(context->mutex);
        return callback(*context);
    }
    catch (const UBAANext::ResultError &error)
    {
        return json_result(failure(error.error()));
    }
    catch (const std::exception &error)
    {
        return error_result(UBAANext::ErrorCode::Unknown, error.what());
    }
    catch (...)
    {
        return error_result(UBAANext::ErrorCode::Unknown, "未知 C ABI 异常");
    }
}

} // namespace

extern "C"
{

    const char *ubaanext_version(void)
    {
        return UBAANEXT_VERSION_STRING;
    }

    int32_t ubaanext_get_capabilities(UbaaNextCapabilities *out_capabilities)
    {
        if (out_capabilities == nullptr)
        {
            return UBAANEXT_STATUS_INVALID_ARGUMENT;
        }

        write_capabilities(current_capabilities(), *out_capabilities);
        return UBAANEXT_STATUS_OK;
    }

    const char *ubaanext_version_info(void)
    {
        return json_result(success(json{{"version", UBAANEXT_VERSION_STRING}}));
    }

    const char *ubaanext_capabilities(void)
    {
        return json_result(
            success(json{{"capabilities", capabilities_to_json(current_capabilities())}}));
    }

    UbaaNextContext *ubaanext_context_create(void)
    {
        try
        {
            UBAANext::Platform::OpenSSL::install_open_ssl_crypto_provider();
            return new (std::nothrow) UbaaNextContext();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void ubaanext_context_release(UbaaNextContext *context)
    {
        delete context;
    }

    int32_t ubaanext_context_set_connection_mode(UbaaNextContext *context, const char *mode)
    {
        if (!context)
            return UBAANEXT_STATUS_INVALID_ARGUMENT;
        auto parsed = parse_connection_mode(mode);
        if (!parsed)
            return UBAANEXT_STATUS_INVALID_CONNECTION_MODE;
        std::lock_guard<std::mutex> lock(context->mutex);
        context->mode = *parsed;
        return UBAANEXT_STATUS_OK;
    }

    void ubaanext_release_result(const char *result_json)
    {
        delete[] result_json;
    }

    const char *ubaanext_auth_login(UbaaNextContext *context, const char *username,
                                    const char *password, const char *captcha)
    {
        return with_context(
            context,
            [username, password, captcha](UbaaNextContext &ctx)
            {
                if (!username || !password)
                {
                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                        "用户名和密码不能为空");
                }
                auto auth = create_auth_service(ctx);
#if UBAANEXT_ENABLE_MOCKS
                if (ctx.mode == UBAANext::ConnectionMode::Mock)
                {
                    return return_service_result(
                        auth.login_mock(c_string_or_empty(username), c_string_or_empty(password)),
                        [](const UBAANext::Model::Account &account)
                        { return json{{"account", account_to_json(account)}}; });
                }
#endif
                if (!ctx.capabilities.live_login || !ctx.capabilities.cookie_persistence)
                {
                    return error_result(UBAANext::ErrorCode::UnsupportedPlatform,
                                        "当前平台未启用可持久化登录能力");
                }
                return return_service_result(
                    auth.login_real(c_string_or_empty(username), c_string_or_empty(password),
                                    ctx.mode, c_string_or_empty(captcha)),
                    [](const UBAANext::Model::Account &account)
                    { return json{{"account", account_to_json(account)}}; });
            });
    }

    const char *ubaanext_auth_logout(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto auth = create_auth_service(ctx);
                                auto result = auth.logout();
                                if (!result)
                                    return json_result(failure(result.error()));
                                return json_result(success(json{{"active", false}}));
                            });
    }

    const char *ubaanext_auth_restore_session(UbaaNextContext *context)
    {
        return with_context(
            context,
            [](UbaaNextContext &ctx)
            {
                auto auth = create_auth_service(ctx);
                return return_service_result(
                    auth.restore_session(), [](const UBAANext::Model::Account &account)
                    { return json{{"active", true}, {"account", account_to_json(account)}}; });
            });
    }

    const char *ubaanext_auth_get_session_state(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto auth = create_auth_service(ctx);
                                auto restored = auth.restore_session();
                                const auto active = static_cast<bool>(restored);
                                json data{{"active", active}, {"mode", mode_to_string(ctx.mode)}};
                                data["account"] =
                                    active ? account_to_json(*restored) : json(nullptr);
                                return json_result(success(std::move(data)));
                            });
    }

    const char *ubaanext_auth_whoami(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto auth = create_auth_service(ctx);
                                auto restored = auth.restore_session();
                                const auto active = static_cast<bool>(restored);
                                json data{{"active", active}, {"mode", mode_to_string(ctx.mode)}};
                                data["account"] =
                                    active ? account_to_json(*restored) : json(nullptr);
                                return json_result(success(std::move(data)));
                            });
    }

    const char *ubaanext_terms(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto service = create_term_service(ctx);
                                return return_service_result(
                                    service.get_terms(),
                                    [](const std::vector<UBAANext::Model::Term> &terms)
                                    { return json{{"terms", array_json(terms, term_to_json)}}; });
                            });
    }

    const char *ubaanext_weeks(UbaaNextContext *context, const char *term_code)
    {
        return with_context(context,
                            [term_code](UbaaNextContext &ctx)
                            {
                                auto service = create_term_service(ctx);
                                return return_service_result(
                                    service.get_weeks(c_string_or_empty(term_code)),
                                    [](const std::vector<UBAANext::Model::Week> &weeks)
                                    { return json{{"weeks", array_json(weeks, week_to_json)}}; });
                            });
    }

    const char *ubaanext_courses_today(UbaaNextContext *context)
    {
        return with_context(
            context,
            [](UbaaNextContext &ctx)
            {
                auto service = create_course_service(ctx);
                return return_service_result(
                    service.get_today_courses(),
                    [](const std::vector<UBAANext::Model::Course> &courses)
                    { return json{{"courses", array_json(courses, course_to_json)}}; });
            });
    }

    const char *ubaanext_courses_date(UbaaNextContext *context, const char *date)
    {
        return with_context(
            context,
            [date](UbaaNextContext &ctx)
            {
                auto service = create_course_service(ctx);
                return return_service_result(
                    service.get_date_courses(c_string_or_empty(date)),
                    [](const std::vector<UBAANext::Model::Course> &courses)
                    { return json{{"courses", array_json(courses, course_to_json)}}; });
            });
    }

    const char *ubaanext_courses_week(UbaaNextContext *context, int32_t week, const char *term_code)
    {
        return with_context(
            context,
            [week, term_code](UbaaNextContext &ctx)
            {
                auto service = create_course_service(ctx);
                const auto term = c_string_or_empty(term_code);
                auto result = term.empty() ? service.get_week_courses(static_cast<int>(week))
                                           : service.get_week_courses(static_cast<int>(week), term);
                return return_service_result(
                    std::move(result), [](const std::vector<UBAANext::Model::Course> &courses)
                    { return json{{"courses", array_json(courses, course_to_json)}}; });
            });
    }

    const char *ubaanext_grades(UbaaNextContext *context, const char *term_code)
    {
        return with_context(
            context,
            [term_code](UbaaNextContext &ctx)
            {
                auto service = create_grade_service(ctx);
                const auto term = c_string_or_empty(term_code);
                auto result = term.empty() ? service.list_all_grades() : service.list_grades(term);
                return return_service_result(
                    std::move(result), [](const std::vector<UBAANext::Model::Grade> &grades)
                    { return json{{"grades", array_json(grades, grade_to_json)}}; });
            });
    }

    const char *ubaanext_exams(UbaaNextContext *context, const char *term_code)
    {
        return with_context(context,
                            [term_code](UbaaNextContext &ctx)
                            {
                                auto service = create_exam_service(ctx);
                                return return_service_result(
                                    service.get_exams(c_string_or_empty(term_code)),
                                    [](const std::vector<UBAANext::Model::Exam> &exams)
                                    { return json{{"exams", array_json(exams, exam_to_json)}}; });
                            });
    }

    const char *ubaanext_todos(UbaaNextContext *context, uint8_t pending_only)
    {
        return with_context(
            context,
            [pending_only](UbaaNextContext &ctx)
            {
                auto service = create_todo_service(ctx);
                UBAANext::TodoQuery query;
                query.pending_only = pending_only != 0;
                return return_service_result(
                    service.list_todos(query),
                    [](const std::vector<UBAANext::Model::FeatureRecord> &todos)
                    { return json{{"todos", array_json(todos, feature_record_to_json)}}; });
            });
    }

    const char *ubaanext_live_week(UbaaNextContext *context, const char *start_date,
                                   const char *end_date)
    {
        return with_context(context,
                            [start_date, end_date](UbaaNextContext &ctx)
                            {
                                auto service = create_live_service(ctx);
                                UBAANext::LiveWeekQuery query;
                                query.start_date = c_string_or_empty(start_date);
                                query.end_date = c_string_or_empty(end_date);
                                return return_service_result(
                                    service.get_week_schedule(query),
                                    [](const UBAANext::Model::LiveWeekSchedule &week)
                                    { return json{{"liveWeek", live_week_to_json(week)}}; });
                            });
    }

    const char *ubaanext_live_resources(UbaaNextContext *context, const char *date,
                                        const char *status, uint8_t from_course)
    {
        return with_context(
            context,
            [date, status, from_course](UbaaNextContext &ctx)
            {
                UBAANext::Model::LiveResourceQuery query;
                query.date = c_string_or_empty(date);
                query.status = c_string_or_empty(status);
                if (query.status.empty())
                    query.status = "all";
                query.from_course = from_course != 0;
                query.page = 1;
                query.size = 100;

                auto service = create_live_service(ctx);
                auto resources = service.resources(query);
                if (!resources)
                    return json_result(failure(resources.error()));

                if (query.from_course)
                {
                    auto filtered = filter_live_from_course(ctx, query.date, std::move(*resources));
                    if (!filtered)
                        return json_result(failure(filtered.error()));
                    return json_result(
                        success(json{{"resources", array_json(*filtered, live_resource_to_json)}}));
                }

                return json_result(
                    success(json{{"resources", array_json(*resources, live_resource_to_json)}}));
            });
    }

    const char *ubaanext_live_detail(UbaaNextContext *context, const char *course_id,
                                     const char *sub_id, const char *date)
    {
        return with_context(context,
                            [course_id, sub_id, date](UbaaNextContext &ctx)
                            {
                                const auto course = c_string_or_empty(course_id);
                                const auto sub = c_string_or_empty(sub_id);
                                if (course.empty() || sub.empty())
                                {
                                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                                        "live detail 需要 course_id 和 sub_id");
                                }

                                auto service = create_live_service(ctx);
                                return return_service_result(
                                    service.detail(course, sub, c_string_or_empty(date)),
                                    [](const UBAANext::Model::LiveResourceDetail &detail)
                                    { return json{{"resource", live_detail_to_json(detail)}}; });
                            });
    }

    const char *ubaanext_signin_today(UbaaNextContext *context)
    {
        return with_context(
            context,
            [](UbaaNextContext &ctx)
            {
                auto service = create_signin_service(ctx);
                return return_service_result(
                    service.list_today_courses(),
                    [](const std::vector<UBAANext::Model::SigninCourse> &courses)
                    { return json{{"courses", array_json(courses, signin_course_to_json)}}; });
            });
    }

    const char *ubaanext_signin_do(UbaaNextContext *context, const char *course_id,
                                   uint8_t confirmed)
    {
        return with_context(
            context,
            [course_id, confirmed](UbaaNextContext &ctx)
            {
                if (!course_id || c_string_or_empty(course_id).empty())
                {
                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                        "签到课程 ID 不能为空");
                }
                auto service = create_signin_service(ctx);
                service.set_write_operation_gate(UBAANext::confirmed_write_operation(
                    ctx.capabilities, "signin do", confirmed != 0));
                return return_service_result(
                    service.perform_signin(c_string_or_empty(course_id)),
                    [](const UBAANext::Model::MutationResult &result)
                    { return json{{"mutation", mutation_result_to_json(result)}}; });
            });
    }

    const char *ubaanext_ygdk_overview(UbaaNextContext *context)
    {
        return with_context(
            context,
            [](UbaaNextContext &ctx)
            {
                auto service = create_ygdk_service(ctx);
                return return_service_result(
                    service.overview_data(),
                    [](const std::pair<UBAANext::Model::YgdkOverview,
                                       std::vector<UBAANext::Model::YgdkItem>> &data)
                    {
                        return json{{"overview", ygdk_overview_to_json(data.first)},
                                    {"items", array_json(data.second, ygdk_item_to_json)}};
                    });
            });
    }

    const char *ubaanext_ygdk_records(UbaaNextContext *context, int32_t page, int32_t size)
    {
        return with_context(
            context,
            [page, size](UbaaNextContext &ctx)
            {
                auto service = create_ygdk_service(ctx);
                return return_service_result(
                    service.record_list(static_cast<int>(page), static_cast<int>(size)),
                    [](const std::vector<UBAANext::Model::YgdkRecord> &records)
                    { return json{{"records", array_json(records, ygdk_record_to_json)}}; });
            });
    }

    const char *ubaanext_feature_list(UbaaNextContext *context, const char *domain,
                                      const char *operation)
    {
        return with_context(
            context,
            [domain, operation](UbaaNextContext &ctx)
            {
                const auto domain_text = c_string_or_empty(domain);
                const auto operation_text = c_string_or_empty(operation);
                if (domain_text.empty() || operation_text.empty())
                {
                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                        "feature list 需要 domain 和 operation");
                }
                auto service = create_feature_service(ctx);
                return return_service_result(
                    service.list(domain_text, operation_text),
                    [](const std::vector<UBAANext::Model::FeatureRecord> &features)
                    { return json{{"features", array_json(features, feature_record_to_json)}}; });
            });
    }

    const char *ubaanext_feature_show(UbaaNextContext *context, const char *domain,
                                      const char *operation, const char *id)
    {
        return with_context(
            context,
            [domain, operation, id](UbaaNextContext &ctx)
            {
                const auto domain_text = c_string_or_empty(domain);
                const auto operation_text = c_string_or_empty(operation);
                if (domain_text.empty() || operation_text.empty())
                {
                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                        "feature show 需要 domain 和 operation");
                }
                auto service = create_feature_service(ctx);
                return return_service_result(
                    service.show(domain_text, operation_text, c_string_or_empty(id)),
                    [](const UBAANext::Model::FeatureRecord &feature)
                    { return json{{"feature", feature_record_to_json(feature)}}; });
            });
    }

    const char *ubaanext_td_status(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto store = ctx.td_store();
                                if (!store)
                                    return json_result(failure(store.error()));
                                auto states = store->load_states();
                                if (!states)
                                    return json_result(failure(states.error()));
                                return json_result(success(json{
                                    {"tdStates", array_json(*states, td_state_to_record_json)}}));
                            });
    }

    const char *ubaanext_td_users(UbaaNextContext *context)
    {
        return with_context(context,
                            [](UbaaNextContext &ctx)
                            {
                                auto store = ctx.td_store();
                                if (!store)
                                    return json_result(failure(store.error()));
                                auto users = store->load_users();
                                if (!users)
                                    return json_result(failure(users.error()));
                                return json_result(success(
                                    json{{"tdUsers", array_json(*users, td_user_to_record_json)}}));
                            });
    }

    const char *ubaanext_td_count_cache(UbaaNextContext *context, const char *student_id)
    {
        return with_context(
            context,
            [student_id](UbaaNextContext &ctx)
            {
                auto store = ctx.td_store();
                if (!store)
                    return json_result(failure(store.error()));
                const auto id = c_string_or_empty(student_id);
                if (!id.empty())
                {
                    auto user = store->load_user(id);
                    if (!user)
                        return json_result(failure(user.error()));
                    if (!*user)
                        return error_result(UBAANext::ErrorCode::InvalidArgument, "TD 用户不存在");
                    auto state = store->load_state(id);
                    if (!state)
                        return json_result(failure(state.error()));
                    return json_result(
                        success(json{{"tdCount", td_count_record_json(**user, *state)}}));
                }

                auto users = store->load_users();
                if (!users)
                    return json_result(failure(users.error()));
                auto states = store->load_states();
                if (!states)
                    return json_result(failure(states.error()));
                json records = json::array();
                for (const auto &user : *users)
                {
                    auto state_it = std::find_if(states->begin(), states->end(),
                                                 [&](const UBAANext::Model::Td::UserState &state)
                                                 { return state.student_id == user.student_id; });
                    std::optional<UBAANext::Model::Td::UserState> state;
                    if (state_it != states->end())
                        state = *state_it;
                    records.push_back(td_count_record_json(user, state));
                }
                return json_result(success(json{{"tdCounts", std::move(records)}}));
            });
    }

    const char *ubaanext_td_image_delete(UbaaNextContext *context, const char *name, uint8_t force,
                                         uint8_t confirmed)
    {
        return with_context(context,
                            [name, force, confirmed](UbaaNextContext &ctx)
                            {
                                const auto image_name = c_string_or_empty(name);
                                if (image_name.empty())
                                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                                        "TD 图片名称不能为空");
                                if (confirmed == 0)
                                    return error_result(UBAANext::ErrorCode::InvalidArgument,
                                                        "TD 图片删除需要 confirmed=true");

                                auto store = ctx.td_store();
                                if (!store)
                                    return json_result(failure(store.error()));
                                auto removed = store->delete_image(image_name, force != 0);
                                if (!removed)
                                    return json_result(failure(removed.error()));

                                UBAANext::Model::MutationResult mutation;
                                mutation.accepted = *removed;
                                mutation.message = *removed ? "TD 图片已删除" : "TD 图片不存在";
                                mutation.summary.id = image_name;
                                mutation.summary.title = image_name;
                                mutation.summary.status = *removed ? "deleted" : "missing";
                                mutation.summary.fields["force"] = force != 0 ? "true" : "false";
                                return json_result(
                                    success(json{{"mutation", mutation_result_to_json(mutation)}}));
                            });
    }
}
