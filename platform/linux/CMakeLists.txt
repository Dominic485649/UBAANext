add_library(UBAANextPlatformLinux INTERFACE)

target_link_libraries(UBAANextPlatformLinux
    INTERFACE
        UBAANextPlatformCurl
        UBAANextPlatformOpenSSL
)
