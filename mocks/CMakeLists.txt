add_library(UBAANextMocks STATIC
    src/MockHttpClient.cpp
    src/MockSecureStore.cpp
    src/MockCacheStore.cpp
)

target_include_directories(UBAANextMocks
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(UBAANextMocks
    PUBLIC UBAANextCore
)

set_target_properties(UBAANextMocks PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
