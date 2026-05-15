add_library(UBAANextCore STATIC
    src/UBAANext.cpp
    src/Auth/Session.cpp
    src/Auth/AuthService.cpp
    src/Auth/SessionManager.cpp
    src/Service/CourseService.cpp
    src/Service/ExamService.cpp
    src/Service/ClassroomService.cpp
    src/Service/TermService.cpp
    src/Service/GradeService.cpp
    src/Service/JudgeService.cpp
    src/Service/SigninService.cpp
    src/Service/LibrarySeatService.cpp
    src/Service/SpocService.cpp
    src/Service/EvaluationService.cpp
    src/Service/VenueReservationService.cpp
    src/Service/YgdkService.cpp
    src/Service/BykcService.cpp
    src/Service/FeatureService.cpp
    src/Service/ResponseUtils.cpp
    src/Protocol/ByxtSession.cpp
    src/Protocol/AppBuaaSession.cpp
    src/Protocol/ScoreSession.cpp
    src/Net/CookieJar.cpp
    src/Net/VpnCipher.cpp
    src/Storage/SecureStoreAdapter.cpp
    src/Storage/MemoryCacheStore.cpp
    src/Parser/JsonParser.cpp
    src/Parser/JudgeParser.cpp
    src/Parser/SpocParser.cpp
    src/Parser/BykcParser.cpp
    src/Parser/EvaluationParser.cpp
    src/Parser/SigninParser.cpp
    src/Parser/YgdkParser.cpp
    src/Parser/VenueReservationParser.cpp
    src/Parser/LibrarySeatParser.cpp
)

target_include_directories(UBAANextCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(UBAANextCore PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(WIN32)
    target_sources(UBAANextCore PRIVATE src/Net/WinHttpClient.cpp)
endif()

target_link_libraries(UBAANextCore
    PUBLIC nlohmann_json::nlohmann_json
)

target_compile_definitions(UBAANextCore
    PUBLIC UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
)

if(WIN32)
    target_link_libraries(UBAANextCore PRIVATE winhttp crypt32 bcrypt)
endif()
