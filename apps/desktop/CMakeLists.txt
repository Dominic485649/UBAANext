find_package(Slint CONFIG QUIET)
if(NOT Slint_FOUND AND UBAANEXT_FETCH_DEPS)
    include(FetchContent)
    FetchContent_Declare(
        Slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG v1.16.1
        SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(Slint)
endif()

if(NOT Slint_FOUND AND NOT TARGET Slint::Slint)
    if(UBAANEXT_REQUIRE_OPTIONAL_DEPS)
        message(FATAL_ERROR "Slint was requested but was not found. Install Slint, provide Slint_DIR, configure with -DUBAANEXT_FETCH_DEPS=ON, or configure with -DUBAANEXT_BUILD_DESKTOP=OFF.")
    else()
        message(WARNING "Slint was requested but was not found; desktop target is disabled.")
        return()
    endif()
endif()

if(WIN32)
    add_executable(UBAANextDesktop WIN32
        src/main.cpp
    )
else()
    add_executable(UBAANextDesktop
        src/main.cpp
    )
endif()

target_link_libraries(UBAANextDesktop
    PRIVATE UBAANextAppRuntime
    PRIVATE UBAANextCliCore
    PRIVATE Slint::Slint
)

if(MSVC)
    target_compile_options(UBAANextDesktop PRIVATE
        /WX-
        /wd4060
        /wd4201
        /wd4244
        /wd4267
        /wd4305
        /wd4458
        /wd4624
    )
else()
    target_compile_options(UBAANextDesktop PRIVATE -Wno-error)
endif()

slint_target_sources(UBAANextDesktop ui/main_window.slint)

set_target_properties(UBAANextDesktop PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)

set(UBAANEXT_DESKTOP_ARCH_DIRECTORY "${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(UBAANEXT_DESKTOP_ARCH_DIRECTORY "x64")
endif()
set(UBAANEXT_DESKTOP_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/${UBAANEXT_DESKTOP_ARCH_DIRECTORY}/$<CONFIG>")
set_target_properties(UBAANextDesktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${UBAANEXT_DESKTOP_OUTPUT_DIRECTORY}"
    PDB_OUTPUT_DIRECTORY "${UBAANEXT_DESKTOP_OUTPUT_DIRECTORY}"
)

if(WIN32)
    set_target_properties(UBAANextDesktop PROPERTIES
        OUTPUT_NAME "ubaa"
        SUFFIX ".exe"
    )
    if(MSVC)
        target_link_options(UBAANextDesktop PRIVATE /ENTRY:mainCRTStartup)
    endif()
else()
    set_target_properties(UBAANextDesktop PROPERTIES
        OUTPUT_NAME "ubaa-gui"
    )
endif()

add_custom_command(TARGET UBAANextDesktop POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DCONFIG=$<CONFIG>
        -DINPUT_FILE=$<TARGET_FILE:UBAANextDesktop>
        -DOUTPUT_FILE=$<TARGET_FILE:UBAANextDesktop>.sha256
        -P "${PROJECT_SOURCE_DIR}/cmake/WriteSha256.cmake"
    VERBATIM
)
