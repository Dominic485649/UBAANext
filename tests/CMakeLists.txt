list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
include(CTest)
include(Catch)

if(UBAANEXT_ENABLE_MOCKS)
    add_executable(UBAANextTests
        unit/ResultTests.cpp
        unit/AuthServiceTests.cpp
        unit/CourseServiceTests.cpp
        unit/SessionManagerTests.cpp
        unit/CookieJarTests.cpp
        unit/SecureStoreAdapterTests.cpp
        unit/ExamServiceTests.cpp
        unit/ClassroomServiceTests.cpp
        unit/CacheStoreTests.cpp
        unit/JsonParserTests.cpp
        unit/JudgeParserTests.cpp
        unit/SpocParserTests.cpp
        unit/BykcParserTests.cpp
        unit/EvaluationParserTests.cpp
        unit/SigninParserTests.cpp
        unit/SigninServiceTests.cpp
        unit/YgdkParserTests.cpp
        unit/VenueReservationParserTests.cpp
        unit/LibrarySeatParserTests.cpp
        unit/ResponseUtilsTests.cpp
        unit/TermServiceTests.cpp
        unit/VpnCipherTests.cpp
    )

    target_link_libraries(UBAANextTests
        PRIVATE UBAANextCore
        PRIVATE UBAANextMocks
        PRIVATE Catch2::Catch2WithMain
    )

    target_compile_definitions(UBAANextTests PRIVATE
        UBAA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )

    set_target_properties(UBAANextTests PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    catch_discover_tests(UBAANextTests)
endif()

# CLI 集成测试
add_executable(UBAANextCliTests
    integration/CliIntegrationTests.cpp
)

target_link_libraries(UBAANextCliTests
    PRIVATE Catch2::Catch2WithMain
    PRIVATE nlohmann_json::nlohmann_json
)

set_target_properties(UBAANextCliTests PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 传递 ubaa 可执行文件路径
target_compile_definitions(UBAANextCliTests PRIVATE
    UBAA_CLI_PATH="$<TARGET_FILE:ubaa>"
    UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
)

catch_discover_tests(UBAANextCliTests)
