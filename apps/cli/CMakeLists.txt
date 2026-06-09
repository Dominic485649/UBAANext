add_library(UBAANextCliCore STATIC
    src/main.cpp
    src/CommandHandlers.cpp
    src/OutputFormatter.cpp
)

add_executable(ubaa
    src/CliMain.cpp
)

target_include_directories(UBAANextCliCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(UBAANextCliCore
    PUBLIC UBAANextAppRuntime
    PRIVATE UBAANextPlatformTcp
)

target_link_libraries(ubaa
    PRIVATE UBAANextCliCore
)

if(UBAANEXT_ENABLE_MOCKS)
    target_link_libraries(UBAANextCliCore PRIVATE UBAANextMocks)
endif()

target_compile_definitions(UBAANextCliCore
    PUBLIC UBAANEXT_ENABLE_MOCKS=$<BOOL:${UBAANEXT_ENABLE_MOCKS}>
)

set(UBAANEXT_CLI_ARCH_DIRECTORY "${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(UBAANEXT_CLI_ARCH_DIRECTORY "x64")
endif()

set(UBAANEXT_CLI_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${UBAANEXT_CLI_ARCH_DIRECTORY}/$<CONFIG>")

set_target_properties(UBAANextCliCore PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)

set_target_properties(ubaa PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    RUNTIME_OUTPUT_DIRECTORY "${UBAANEXT_CLI_OUTPUT_DIRECTORY}"
    PDB_OUTPUT_DIRECTORY "${UBAANEXT_CLI_OUTPUT_DIRECTORY}"
)

if(WIN32)
    set_target_properties(ubaa PROPERTIES
        OUTPUT_NAME "ubaa"
        SUFFIX ".com"
    )
else()
    set_target_properties(ubaa PROPERTIES
        OUTPUT_NAME "ubaa"
    )
endif()

add_custom_command(TARGET ubaa POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DCONFIG=$<CONFIG>
        -DINPUT_FILE=$<TARGET_FILE:ubaa>
        -DOUTPUT_FILE=$<TARGET_FILE:ubaa>.sha256
        -P "${PROJECT_SOURCE_DIR}/cmake/WriteSha256.cmake"
    VERBATIM
)
