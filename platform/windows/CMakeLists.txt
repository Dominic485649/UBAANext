if(WIN32)
    add_library(UBAANextPlatformWindows STATIC
        src/DpapiSecureStore.cpp
        src/WindowsAppDataPathProvider.cpp
        src/WindowsNetworkEnvironment.cpp
        src/WindowsPlatformCapabilities.cpp
        src/WinFspMount.cpp
    )

    target_include_directories(UBAANextPlatformWindows
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(UBAANextPlatformWindows
        PUBLIC UBAANextCore
        PRIVATE
            UBAANextPlatformCurl
            UBAANextPlatformOpenSSL
            UBAANextPlatformTcp
            crypt32
            ws2_32
    )
    target_compile_definitions(UBAANextPlatformWindows
        PUBLIC
            UBAANEXT_BUILD_DESKTOP=$<BOOL:${UBAANEXT_BUILD_DESKTOP}>
            UBAANEXT_ENABLE_WINFSP=$<BOOL:${UBAANEXT_HAS_WINFSP}>
            UBAANEXT_ENABLE_CLOUD_FILES=$<BOOL:${UBAANEXT_HAS_CLOUD_FILES}>
    )

    if(UBAANEXT_HAS_WINFSP)
        target_link_libraries(UBAANextPlatformWindows PRIVATE UBAANextOptionalWinFsp)
    endif()
    if(UBAANEXT_HAS_CLOUD_FILES)
        target_link_libraries(UBAANextPlatformWindows PRIVATE UBAANextOptionalCloudFiles)
    endif()

    set_target_properties(UBAANextPlatformWindows PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
else()
    add_library(UBAANextPlatformWindows INTERFACE)
endif()
