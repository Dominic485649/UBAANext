include(FetchContent)

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    if(DEFINED UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR AND EXISTS "${UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
        add_library(nlohmann_json INTERFACE)
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        target_include_directories(nlohmann_json INTERFACE "${UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR}")
    elseif(UBAANEXT_FETCH_DEPS)
        FetchContent_Declare(
            nlohmann_json
            URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
            URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
        )
        FetchContent_MakeAvailable(nlohmann_json)
    else()
        message(FATAL_ERROR "nlohmann_json not found. Install it, set UBAANEXT_NLOHMANN_JSON_INCLUDE_DIR, or configure with -DUBAANEXT_FETCH_DEPS=ON.")
    endif()
endif()

find_package(OpenSSL CONFIG QUIET)
if(NOT OpenSSL_FOUND)
    find_package(OpenSSL QUIET)
endif()
if(NOT OpenSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found. Install it or configure with the vcpkg toolchain.")
endif()

if(UBAANEXT_BUILD_TESTS)
    find_package(Catch2 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        if(NOT UBAANEXT_FETCH_DEPS)
            message(FATAL_ERROR "Catch2 not found. Install it or configure with -DUBAANEXT_FETCH_DEPS=ON.")
        endif()
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.14.0
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()
