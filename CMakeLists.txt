cmake_minimum_required(VERSION 3.21)

include(cmake/UBAANextOptions.cmake)

project(UBAANext
    VERSION 0.3.0
    LANGUAGES CXX
    DESCRIPTION "UBAA Next - C++ native campus service client"
)

set(UBAANEXT_CXX_STANDARD "17" CACHE STRING "C++ standard used by UBAA Next (17 or 20)")
set_property(CACHE UBAANEXT_CXX_STANDARD PROPERTY STRINGS 17 20)
if(NOT UBAANEXT_CXX_STANDARD MATCHES "^(17|20)$")
    message(FATAL_ERROR "UBAANEXT_CXX_STANDARD must be 17 or 20")
endif()

set(CMAKE_CXX_STANDARD ${UBAANEXT_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 自动生成 Version.hpp，确保版本号与 project() 声明同步
configure_file(
    ${CMAKE_SOURCE_DIR}/core/include/UBAANext/Version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/UBAANext/Version.hpp
    @ONLY
)

include(cmake/UBAANextWarnings.cmake)
include(cmake/Dependencies.cmake)

if(UBAANEXT_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(core)
add_subdirectory(mocks)
add_subdirectory(apps/cli)

if(UBAANEXT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
