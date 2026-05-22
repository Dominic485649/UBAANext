add_library(UBAANextPlatformOpenSSL STATIC
    src/OpenSslCryptoInstaller.cpp
    src/OpenSslCryptoProvider.cpp
    src/OpenSslErrorMapper.cpp
    src/OpenSslRuntime.cpp
)

target_include_directories(UBAANextPlatformOpenSSL
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(UBAANextPlatformOpenSSL
    PUBLIC UBAANextCore
    PRIVATE OpenSSL::Crypto
)

set_target_properties(UBAANextPlatformOpenSSL PROPERTIES
    CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)
