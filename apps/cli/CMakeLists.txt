add_executable(ubaa
    src/main.cpp
    src/PlainFileStore.cpp
    src/ServiceFactory.cpp
    src/OutputFormatter.cpp
    src/CliConfig.cpp
    src/SecurityRedaction.cpp
)

target_include_directories(ubaa
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ubaa
    PRIVATE UBAANextCore
)

if(UBAANEXT_ENABLE_MOCKS)
    target_link_libraries(ubaa PRIVATE UBAANextMocks)
endif()

target_compile_definitions(ubaa
    PRIVATE UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
)

if(WIN32)
    target_link_libraries(ubaa PRIVATE crypt32)
endif()

set_target_properties(ubaa PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
