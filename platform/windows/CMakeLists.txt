add_library(UBAANextPlatformWindows INTERFACE)

target_link_libraries(UBAANextPlatformWindows
    INTERFACE
        UBAANextPlatformCurl
        UBAANextPlatformOpenSSL
)
