function(ubaanext_optional_dependency_missing dependency_name disable_message)
    if(UBAANEXT_REQUIRE_OPTIONAL_DEPS)
        message(FATAL_ERROR "${dependency_name} was requested but was not found. ${disable_message}")
    else()
        message(WARNING "${dependency_name} was requested but was not found; support is disabled. ${disable_message}")
    endif()
endfunction()

set(UBAANEXT_HAS_WINFSP OFF)
set(UBAANEXT_HAS_CLOUD_FILES OFF)
set(UBAANEXT_HAS_FUSE OFF)

if(WIN32 AND UBAANEXT_ENABLE_WINFSP)
    find_path(UBAANEXT_WINFSP_INCLUDE_DIR winfsp/winfsp.h
        HINTS
            "$ENV{WINFSP_HOME}/inc"
            "$ENV{ProgramFiles}/WinFsp/inc"
            "$ENV{ProgramFiles(x86)}/WinFsp/inc"
    )
    find_library(UBAANEXT_WINFSP_LIBRARY
        NAMES winfsp-x64 winfsp
        HINTS
            "$ENV{WINFSP_HOME}/lib"
            "$ENV{ProgramFiles}/WinFsp/lib"
            "$ENV{ProgramFiles(x86)}/WinFsp/lib"
    )
    if(UBAANEXT_WINFSP_INCLUDE_DIR AND UBAANEXT_WINFSP_LIBRARY)
        set(UBAANEXT_HAS_WINFSP ON)
        add_library(UBAANextOptionalWinFsp INTERFACE)
        target_include_directories(UBAANextOptionalWinFsp INTERFACE "${UBAANEXT_WINFSP_INCLUDE_DIR}")
        target_link_libraries(UBAANextOptionalWinFsp INTERFACE "${UBAANEXT_WINFSP_LIBRARY}")
    else()
        ubaanext_optional_dependency_missing("WinFsp" "Install the WinFsp SDK/runtime, set WINFSP_HOME, or configure with -DUBAANEXT_ENABLE_WINFSP=OFF.")
    endif()
endif()

if(WIN32 AND UBAANEXT_ENABLE_CLOUD_FILES)
    find_path(UBAANEXT_CLOUD_FILES_INCLUDE_DIR cfapi.h)
    find_library(UBAANEXT_CLOUD_FILES_LIBRARY NAMES CfApi cfapi)
    if(UBAANEXT_CLOUD_FILES_INCLUDE_DIR AND UBAANEXT_CLOUD_FILES_LIBRARY)
        set(UBAANEXT_HAS_CLOUD_FILES ON)
        add_library(UBAANextOptionalCloudFiles INTERFACE)
        target_include_directories(UBAANextOptionalCloudFiles INTERFACE "${UBAANEXT_CLOUD_FILES_INCLUDE_DIR}")
        target_link_libraries(UBAANextOptionalCloudFiles INTERFACE "${UBAANEXT_CLOUD_FILES_LIBRARY}")
    else()
        ubaanext_optional_dependency_missing("Windows Cloud Files" "Install a Windows SDK with cfapi.h/CfApi.lib or configure with -DUBAANEXT_ENABLE_CLOUD_FILES=OFF.")
    endif()
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND UBAANEXT_ENABLE_FUSE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FUSE3 QUIET IMPORTED_TARGET fuse3)
        if(FUSE3_FOUND)
            set(UBAANEXT_HAS_FUSE ON)
            add_library(UBAANextOptionalFuse INTERFACE)
            target_link_libraries(UBAANextOptionalFuse INTERFACE PkgConfig::FUSE3)
        endif()
    endif()
    if(NOT UBAANEXT_HAS_FUSE)
        ubaanext_optional_dependency_missing("FUSE" "Install libfuse3 development files or configure with -DUBAANEXT_ENABLE_FUSE=OFF.")
    endif()
endif()
