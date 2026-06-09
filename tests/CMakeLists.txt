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
        unit/LiveServiceTests.cpp
        unit/CloudServiceTests.cpp
        unit/CloudVfsTests.cpp
        unit/FuseMountAdapterTests.cpp
        unit/AppRuntimeTests.cpp
        unit/SrsServiceTests.cpp
        unit/WifiServiceTests.cpp
        unit/ResponseUtilsTests.cpp
        unit/TdModelTests.cpp
        unit/TdStoreTests.cpp
        unit/TdClientTests.cpp
        unit/TdTcpTransportTests.cpp
        unit/TdServiceTests.cpp
        unit/TdSchedulerServiceTests.cpp
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
        PRIVATE UBAANextAppRuntime
        PRIVATE UBAANextPlatformCurl
        PRIVATE UBAANextPlatformOpenSSL
        PRIVATE UBAANextPlatformTcp
        PRIVATE UBAANextMocks
        PRIVATE Catch2::Catch2
    )

    if(WIN32)
        target_link_libraries(UBAANextTests PRIVATE ws2_32)
    endif()

    target_include_directories(UBAANextTests
        PRIVATE ${CMAKE_SOURCE_DIR}/apps/cli/include
        PRIVATE ${CMAKE_SOURCE_DIR}/apps/runtime/include
    )

    target_compile_definitions(UBAANextTests PRIVATE
        UBAA_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        UBAANEXT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        UBAANEXT_ENABLE_WINFSP=$<BOOL:${UBAANEXT_HAS_WINFSP}>
        UBAANEXT_ENABLE_FUSE=$<BOOL:${UBAANEXT_HAS_FUSE}>
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
    PRIVATE UBAANextCliCore
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
    UBAANEXT_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
if(TARGET UBAANextDesktop)
    add_dependencies(UBAANextCliTests UBAANextDesktop)
    target_compile_definitions(UBAANextCliTests PRIVATE
        UBAANEXT_HAS_DESKTOP=1
        UBAANEXT_DESKTOP_PATH="$<TARGET_FILE:UBAANextDesktop>"
    )
else()
    target_compile_definitions(UBAANextCliTests PRIVATE UBAANEXT_HAS_DESKTOP=0)
endif()

catch_discover_tests(UBAANextCliTests)

if(UBAANEXT_BUILD_BINDINGS)
    add_executable(UBAANextCAbiSmokeTests
        integration/CAbiSmokeTests.cpp
    )

    target_link_libraries(UBAANextCAbiSmokeTests
        PRIVATE UBAANextBindingsC
        PRIVATE Catch2::Catch2WithMain
        PRIVATE nlohmann_json::nlohmann_json
    )

    target_compile_definitions(UBAANextCAbiSmokeTests PRIVATE
        UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
    )

    set_target_properties(UBAANextCAbiSmokeTests PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(WIN32)
        add_custom_command(TARGET UBAANextCAbiSmokeTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:UBAANextBindingsC>
                $<TARGET_FILE_DIR:UBAANextCAbiSmokeTests>
            COMMENT "Copy C ABI DLL next to smoke tests"
            VERBATIM
        )
    endif()

    catch_discover_tests(UBAANextCAbiSmokeTests)
endif()
