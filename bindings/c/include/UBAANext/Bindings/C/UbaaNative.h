#pragma once

#include <stdint.h>

#ifdef _WIN32
#ifdef UBAANEXT_BINDINGS_C_EXPORTS
#define UBAANEXT_C_API __declspec(dllexport)
#else
#define UBAANEXT_C_API __declspec(dllimport)
#endif
#else
#define UBAANEXT_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum UbaaNextStatus
    {
        UBAANEXT_STATUS_OK = 0,
        UBAANEXT_STATUS_INVALID_ARGUMENT = -1,
        UBAANEXT_STATUS_INVALID_CONNECTION_MODE = -2
    } UbaaNextStatus;

    typedef struct UbaaNextCapabilities
    {
        uint8_t real_network;
        uint8_t secure_cookie_persistence;
        uint8_t cookie_persistence;
        uint8_t redirect_control;
        uint8_t openssl_crypto;
        uint8_t secure_store;
        uint8_t app_data_path;
        uint8_t upload_bytes;
        uint8_t live_login;
        uint8_t write_operations;
        uint8_t reserved[14]; /* 为 ABI 兼容扩展预留；写入方必须置零。 */
    } UbaaNextCapabilities;

    typedef struct UbaaNextContext UbaaNextContext;

    /**
     * Stable C ABI entry for embedding clients. Returns the native SDK version string without
     * remote I/O, local file reads, or writes.
     */
    UBAANEXT_C_API const char *ubaanext_version(void);
    UBAANEXT_C_API const char *ubaanext_version_info(void);

    /**
     * Stable C ABI entry for embedding clients. Writes current platform capability flags;
     * unsupported, fallback, and gated flags must not be treated as completion. Returns
     * UBAANEXT_STATUS_OK or UBAANEXT_STATUS_INVALID_ARGUMENT.
     */
    UBAANEXT_C_API int32_t ubaanext_get_capabilities(UbaaNextCapabilities *out_capabilities);
    UBAANEXT_C_API const char *ubaanext_capabilities(void);

    UBAANEXT_C_API UbaaNextContext *ubaanext_context_create(void);
    UBAANEXT_C_API void ubaanext_context_release(UbaaNextContext *context);
    /**
     * Sets context connection mode to mock, direct, vpn, or webvpn.
     * Returns UBAANEXT_STATUS_OK, UBAANEXT_STATUS_INVALID_ARGUMENT, or
     * UBAANEXT_STATUS_INVALID_CONNECTION_MODE.
     */
    UBAANEXT_C_API int32_t ubaanext_context_set_connection_mode(UbaaNextContext *context,
                                                                const char *mode);
    UBAANEXT_C_API void ubaanext_release_result(const char *result_json);

    UBAANEXT_C_API const char *ubaanext_auth_login(UbaaNextContext *context, const char *username,
                                                   const char *password, const char *captcha);
    UBAANEXT_C_API const char *ubaanext_auth_logout(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_auth_restore_session(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_auth_get_session_state(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_auth_whoami(UbaaNextContext *context);

    UBAANEXT_C_API const char *ubaanext_terms(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_weeks(UbaaNextContext *context, const char *term_code);
    UBAANEXT_C_API const char *ubaanext_courses_today(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_courses_date(UbaaNextContext *context, const char *date);
    UBAANEXT_C_API const char *ubaanext_courses_week(UbaaNextContext *context, int32_t week,
                                                     const char *term_code);
    UBAANEXT_C_API const char *ubaanext_grades(UbaaNextContext *context, const char *term_code);
    UBAANEXT_C_API const char *ubaanext_exams(UbaaNextContext *context, const char *term_code);
    UBAANEXT_C_API const char *ubaanext_todos(UbaaNextContext *context, uint8_t pending_only);
    UBAANEXT_C_API const char *ubaanext_live_week(UbaaNextContext *context, const char *start_date,
                                                  const char *end_date);
    UBAANEXT_C_API const char *ubaanext_live_resources(UbaaNextContext *context, const char *date,
                                                       const char *status, uint8_t from_course);
    UBAANEXT_C_API const char *ubaanext_live_detail(UbaaNextContext *context, const char *course_id,
                                                    const char *sub_id, const char *date);
    UBAANEXT_C_API const char *ubaanext_signin_today(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_signin_do(UbaaNextContext *context, const char *course_id,
                                                  uint8_t confirmed);
    UBAANEXT_C_API const char *ubaanext_ygdk_overview(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_ygdk_records(UbaaNextContext *context, int32_t page,
                                                     int32_t size);
    UBAANEXT_C_API const char *ubaanext_feature_list(UbaaNextContext *context, const char *domain,
                                                     const char *operation);
    UBAANEXT_C_API const char *ubaanext_feature_show(UbaaNextContext *context, const char *domain,
                                                     const char *operation, const char *id);
    UBAANEXT_C_API const char *ubaanext_td_status(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_td_users(UbaaNextContext *context);
    UBAANEXT_C_API const char *ubaanext_td_count_cache(UbaaNextContext *context,
                                                       const char *student_id);
    UBAANEXT_C_API const char *ubaanext_td_image_delete(UbaaNextContext *context, const char *name,
                                                        uint8_t force, uint8_t confirmed);

#ifdef __cplusplus
}
#endif
