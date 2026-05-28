list(APPEND CMAKE_MODULE_PATH ${Catch2_DIR})
include(CTest)
include(Catch)

if(UBAANEXT_ENABLE_MOCKS)
    add_executable(UBAANextTests
        unit/TestMain.cpp
        unit/ResultTests.cpp
        unit/AuthServiceTests.cpp
        unit/CourseServiceTests.cpp
        unit/SessionManagerTests.cpp
        unit/CookieJarTests.cpp
        unit/CurlCookieStoreTests.cpp
        unit/CurlHttpClientTests.cpp
        unit/SecureStoreAdapterTests.cpp
        unit/SecurityRedactionTests.cpp
        unit/ExamServiceTests.cpp
        unit/FeatureServiceTests.cpp
        unit/ClassroomServiceTests.cpp
        unit/CacheStoreTests.cpp
        unit/CryptoProviderTests.cpp
        unit/CryptoInjectionTests.cpp
        unit/HarmonyPlatformTests.cpp
        unit/LinuxPlatformTests.cpp
        unit/PublicApiTests.cpp
        unit/JsonParserTests.cpp
        unit/JudgeParserTests.cpp
        unit/SpocParserTests.cpp
        unit/SpocServiceTests.cpp
        unit/BykcParserTests.cpp
        unit/BykcServiceTests.cpp
        unit/EvaluationParserTests.cpp
        unit/EvaluationServiceTests.cpp
        unit/SigninParserTests.cpp
        unit/SigninServiceTests.cpp
        unit/WriteOperationGateTests.cpp
        unit/JudgeServiceTests.cpp
        unit/YgdkParserTests.cpp
        unit/YgdkServiceTests.cpp
        unit/VenueReservationParserTests.cpp
        unit/VenueReservationServiceTests.cpp
        unit/LibrarySeatParserTests.cpp
        unit/LibrarySeatServiceTests.cpp
        unit/ResponseUtilsTests.cpp
        unit/DownstreamSessionTypesTests.cpp
        unit/SessionGuardsTests.cpp
        unit/RedirectNavigatorTests.cpp
        unit/CasFormParserTests.cpp
        unit/AuthorizedDownstreamRequestExecutorTests.cpp
        unit/TermServiceTests.cpp
        unit/P1ReadonlyContractTests.cpp
        unit/PlatformBoundaryTests.cpp
        unit/TodoServiceTests.cpp
        unit/VpnCipherTests.cpp
    )

    target_link_libraries(UBAANextTests
        PRIVATE UBAANextCore
        PRIVATE UBAANextPlatformCurl
        PRIVATE UBAANextPlatformOpenSSL
        PRIVATE UBAANextMocks
        PRIVATE Catch2::Catch2
    )

    if(WIN32)
        target_link_libraries(UBAANextTests PRIVATE ws2_32)
    endif()

    target_sources(UBAANextTests
        PRIVATE ${CMAKE_SOURCE_DIR}/apps/cli/src/SecurityRedaction.cpp
    )

    target_include_directories(UBAANextTests
        PRIVATE ${CMAKE_SOURCE_DIR}/apps/cli/include
    )

    target_compile_definitions(UBAANextTests PRIVATE
        UBAA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        UBAANEXT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )

    set_target_properties(UBAANextTests PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    catch_discover_tests(UBAANextTests)
endif()

# CLI 集成测试
set(UBAANEXT_CLI_TEST_SOURCES
    integration/CliIntegrationTests.cpp
)
if(UBAANEXT_BUILD_LIVE_SMOKE)
    list(APPEND UBAANEXT_CLI_TEST_SOURCES integration/LiveSmokeTests.cpp)
endif()

add_executable(UBAANextCliTests
    ${UBAANEXT_CLI_TEST_SOURCES}
)

add_dependencies(UBAANextCliTests ubaa)

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
