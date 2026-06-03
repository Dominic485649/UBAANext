if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_library(UBAANextPlatformHarmony STATIC
        src/HarmonyAppDataPathProvider.cpp
        src/HarmonyPlatformCapabilities.cpp
        src/UnsupportedSecureStore.cpp
    )

    target_include_directories(UBAANextPlatformHarmony
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(UBAANextPlatformHarmony
        PUBLIC UBAANextCore
        PRIVATE
            UBAANextPlatformCurl
            UBAANextPlatformOpenSSL
            UBAANextPlatformTcp
    )

    set_target_properties(UBAANextPlatformHarmony PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
else()
    add_library(UBAANextPlatformHarmony INTERFACE)
    target_link_libraries(UBAANextPlatformHarmony
        INTERFACE
            UBAANextPlatformCurl
            UBAANextPlatformOpenSSL
            UBAANextPlatformTcp
    )
endif()
