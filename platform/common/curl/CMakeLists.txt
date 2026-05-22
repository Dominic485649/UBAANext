add_library(UBAANextPlatformCurl STATIC
    src/CurlCookieStore.cpp
    src/CurlErrorMapper.cpp
    src/CurlHttpClient.cpp
    src/CurlNetworkStack.cpp
    src/CurlRedirectController.cpp
    src/CurlRuntime.cpp
)

target_include_directories(UBAANextPlatformCurl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(UBAANextPlatformCurl
    PUBLIC UBAANextCore
    PRIVATE CURL::libcurl
)

set_target_properties(UBAANextPlatformCurl PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)
