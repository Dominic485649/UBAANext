add_library(UBAANextAppRuntime STATIC
    ../cli/src/CliConfig.cpp
    ../cli/src/PlainFileStore.cpp
    ../cli/src/PlatformContextFactory.cpp
    ../cli/src/SecurityRedaction.cpp
    ../cli/src/ServiceFactory.cpp
    src/AppRuntime.cpp
    src/CloudMountManager.cpp
    src/Mount/WinFspMountAdapter.cpp
    src/Mount/CloudFilesMountAdapterStub.cpp
    src/Mount/FuseMountAdapter.cpp
)

target_include_directories(UBAANextAppRuntime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/apps/cli/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(UBAANextAppRuntime
    PUBLIC UBAANextCore
    PUBLIC UBAANextPlatformCurrent
    PRIVATE UBAANextPlatformCurl
    PRIVATE UBAANextPlatformOpenSSL
    PRIVATE UBAANextPlatformTcp
)

if(UBAANEXT_ENABLE_MOCKS)
    target_link_libraries(UBAANextAppRuntime PRIVATE UBAANextMocks)
endif()

if(UBAANEXT_HAS_CLOUD_FILES)
    target_sources(UBAANextAppRuntime PRIVATE platform/windows/src/CloudFilesMountAdapter.cpp)
    target_link_libraries(UBAANextAppRuntime PRIVATE UBAANextOptionalCloudFiles)
    set_source_files_properties(src/Mount/CloudFilesMountAdapterStub.cpp PROPERTIES HEADER_FILE_ONLY ON)
endif()
target_compile_definitions(UBAANextAppRuntime
    PUBLIC
        UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
        UBAANEXT_ENABLE_WINFSP=$<BOOL:${UBAANEXT_HAS_WINFSP}>
        UBAANEXT_ENABLE_CLOUD_FILES=$<BOOL:${UBAANEXT_HAS_CLOUD_FILES}>
        UBAANEXT_ENABLE_FUSE=$<BOOL:${UBAANEXT_HAS_FUSE}>
)

set_target_properties(UBAANextAppRuntime PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)
