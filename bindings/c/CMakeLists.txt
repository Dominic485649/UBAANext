add_library(UBAANextBindingsC SHARED
    src/UbaaNative.cpp
)

target_include_directories(UBAANextBindingsC
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(UBAANextBindingsC
    PRIVATE
        UBAANextCore
        UBAANextPlatformCurrent
        UBAANextPlatformCurl
        UBAANextPlatformOpenSSL
)

target_compile_definitions(UBAANextBindingsC
    PRIVATE UBAANEXT_BINDINGS_C_EXPORTS
)

set_target_properties(UBAANextBindingsC PROPERTIES
    OUTPUT_NAME ubaanext_c
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
