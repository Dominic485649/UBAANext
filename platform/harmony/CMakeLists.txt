add_library(UBAANextPlatformHarmony INTERFACE)

target_link_libraries(UBAANextPlatformHarmony
    INTERFACE
        UBAANextPlatformCurl
        UBAANextPlatformOpenSSL
)
