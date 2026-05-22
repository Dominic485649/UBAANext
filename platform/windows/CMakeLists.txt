if(WIN32)
    add_library(UBAANextPlatformWindows STATIC
        src/DpapiSecureStore.cpp
        src/WindowsAppDataPathProvider.cpp
        src/WindowsPlatformCapabilities.cpp
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
            crypt32
    )

    set_target_properties(UBAANextPlatformWindows PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
    )
else()
    add_library(UBAANextPlatformWindows INTERFACE)
endif()
