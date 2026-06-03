add_library(UBAANextPlatformTcp STATIC
    src/TdTcpTransport.cpp
)

target_include_directories(UBAANextPlatformTcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(UBAANextPlatformTcp
    PUBLIC UBAANextCore
)

if(WIN32)
    target_link_libraries(UBAANextPlatformTcp PRIVATE ws2_32)
endif()

set_target_properties(UBAANextPlatformTcp PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
