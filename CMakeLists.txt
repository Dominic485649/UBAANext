cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

include(cmake/UBAANextOptions.cmake)

project(UBAANext
    VERSION 0.4.0
    LANGUAGES CXX
    DESCRIPTION "UBAA Next - C++ native campus service client"
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(UBAANEXT_CXX_STANDARD "17" CACHE STRING "C++ standard used by UBAA Next (17 or 20)")
set_property(CACHE UBAANEXT_CXX_STANDARD PROPERTY STRINGS 17 20)
if(NOT UBAANEXT_CXX_STANDARD MATCHES "^(17|20)$")
    message(FATAL_ERROR "UBAANEXT_CXX_STANDARD must be 17 or 20")
endif()

set(CMAKE_CXX_STANDARD ${UBAANEXT_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(UBAANEXT_BUILD_BINDINGS AND NOT WIN32)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# 自动生成 Version.hpp，确保版本号与 project() 声明同步
configure_file(
    ${CMAKE_SOURCE_DIR}/core/include/UBAANext/Version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/UBAANext/Version.hpp
    @ONLY
)

include(cmake/UBAANextWarnings.cmake)

add_custom_target(ubaanext-clean-local
    COMMAND ${CMAKE_COMMAND}
        -DUBAANEXT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${PROJECT_SOURCE_DIR}/cmake/CleanLocalArtifacts.cmake
    COMMENT "Clean UBAA Next local build artifacts while keeping external dependency caches"
    VERBATIM
)

include(cmake/Dependencies.cmake)

if(UBAANEXT_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)
add_subdirectory(platform)
if(UBAANEXT_ENABLE_MOCKS)
    add_subdirectory(mocks)
endif()
if(UBAANEXT_BUILD_BINDINGS)
    add_subdirectory(bindings)
endif()
if(UBAANEXT_BUILD_CLI)
    add_subdirectory(apps/cli)
endif()

if(UBAANEXT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

install(DIRECTORY core/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_BINARY_DIR}/generated/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/common/curl/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/common/openssl/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/common/tcp/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/harmony/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/linux/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY platform/windows/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(UBAANEXT_BUILD_BINDINGS)
    install(DIRECTORY bindings/c/include/UBAANext DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

set(UBAANEXT_INSTALL_TARGETS
    UBAANextCore
    UBAANextPlatformCurl
    UBAANextPlatformOpenSSL
    UBAANextPlatformTcp
    UBAANextPlatformMock
    UBAANextPlatformWindows
    UBAANextPlatformLinux
    UBAANextPlatformHarmony
    UBAANextPlatformCurrent
)
if(UBAANEXT_BUILD_BINDINGS)
    list(APPEND UBAANEXT_INSTALL_TARGETS UBAANextBindingsC)
endif()

install(TARGETS ${UBAANEXT_INSTALL_TARGETS}
    EXPORT UBAANextTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/UBAANextConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/UBAANextConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/UBAANextConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UBAANext
)

install(EXPORT UBAANextTargets
    NAMESPACE UBAANext::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UBAANext
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/UBAANextConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/UBAANextConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UBAANext
)
