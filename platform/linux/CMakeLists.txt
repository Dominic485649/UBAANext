if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(UBAANEXT_LINUX_SOURCES
        src/LinuxAppDataPathProvider.cpp
        src/LinuxFuseMount.cpp
        src/LinuxNetworkEnvironment.cpp
        src/LinuxPlatformCapabilities.cpp
        src/LocalEncryptedFileStore.cpp
        src/SecretServiceSecureStore.cpp
        src/UnsupportedSecureStore.cpp
    )

    add_library(UBAANextPlatformLinux STATIC
        ${UBAANEXT_LINUX_SOURCES}
    )

    target_include_directories(UBAANextPlatformLinux
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(UBAANextPlatformLinux
        PUBLIC UBAANextCore
        PRIVATE
            UBAANextPlatformCurl
            UBAANextPlatformOpenSSL
            UBAANextPlatformTcp
            OpenSSL::Crypto
    )
    if(UBAANEXT_ENABLE_LINUX_LIBSECRET)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)
        target_link_libraries(UBAANextPlatformLinux PRIVATE PkgConfig::LIBSECRET)
        target_compile_definitions(UBAANextPlatformLinux PUBLIC UBAANEXT_ENABLE_LINUX_LIBSECRET=1)
    else()
        target_compile_definitions(UBAANextPlatformLinux PUBLIC UBAANEXT_ENABLE_LINUX_LIBSECRET=0)
    endif()
    target_compile_definitions(UBAANextPlatformLinux
        PUBLIC
            UBAANEXT_BUILD_DESKTOP=$<BOOL:${UBAANEXT_BUILD_DESKTOP}>
            UBAANEXT_ENABLE_FUSE=$<BOOL:${UBAANEXT_HAS_FUSE}>
    )
    if(UBAANEXT_HAS_FUSE)
        target_link_libraries(UBAANextPlatformLinux PRIVATE UBAANextOptionalFuse)
    endif()

    set_target_properties(UBAANextPlatformLinux PROPERTIES
        CXX_STANDARD ${UBAANEXT_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
else()
    add_library(UBAANextPlatformLinux INTERFACE)
    target_link_libraries(UBAANextPlatformLinux
        INTERFACE
            UBAANextPlatformCurl
            UBAANextPlatformOpenSSL
            UBAANextPlatformTcp
    )
endif()
