add_library(UBAANextPlatformMock INTERFACE)

target_link_libraries(UBAANextPlatformMock
    INTERFACE
        UBAANextCore
        UBAANextPlatformTcp
)
