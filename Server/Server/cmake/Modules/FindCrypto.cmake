# - Find Crypto library (BoringSSL or OpenSSL)
#
# Usage:
#     find_package(Crypto REQUIRED)
#
# Variables defined by this module:
#
#  Crypto_FOUND               Found crypto libraries
#  Crypto_INCLUDE_DIR         The include directory
#  Crypto_SSL_LIBRARY         The SSL static library
#  Crypto_CRYPTO_LIBRARY      The crypto static library
#  openssl::ssl::static       IMPORTED SSL target
#  openssl::crypto::static    IMPORTED crypto target
#
# Prefers BoringSSL (set via Crypto_ROOT_DIR or BoringSSL_ROOT_DIR) over OpenSSL.

include(FindPackageHandleStandardArgs)

if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE $ENV{build_os_type})
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif()
if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE "linux")
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH "amd64")
endif()

# Search paths: BoringSSL first, then OpenSSL prebuild, then system
set(_crypto_search_paths
    ${Crypto_ROOT_DIR}
    ${BoringSSL_ROOT_DIR}/lib
    ${LIBRARY_PATH}/boringssl/lib
    ${LIBRARY_PATH}/openssl-prebuild/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/lib
    /usr/local/lib
    /usr/lib
)

set(_crypto_include_paths
    ${BoringSSL_ROOT_DIR}/include
    ${BoringSSL_ROOT_DIR}/../include
    ${LIBRARY_PATH}/boringssl/include
    ${LIBRARY_PATH}/boringssl/src/include
    ${LIBRARY_PATH}/openssl-prebuild/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    /usr/local/include
    /usr/include
)

find_path(Crypto_INCLUDE_DIR
    NAMES openssl/ssl.h
    PATHS ${_crypto_include_paths}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

# Prefer static .a over shared .so
find_library(Crypto_SSL_LIBRARY
    NAMES libssl.a ssl
    PATHS ${_crypto_search_paths}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(Crypto_CRYPTO_LIBRARY
    NAMES libcrypto.a crypto
    PATHS ${_crypto_search_paths}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT Crypto_INCLUDE_DIR)
    find_path(Crypto_INCLUDE_DIR NAMES openssl/ssl.h)
endif()
if(NOT Crypto_SSL_LIBRARY)
    find_library(Crypto_SSL_LIBRARY NAMES ssl)
endif()
if(NOT Crypto_CRYPTO_LIBRARY)
    find_library(Crypto_CRYPTO_LIBRARY NAMES crypto)
endif()

if(Crypto_INCLUDE_DIR AND Crypto_SSL_LIBRARY AND Crypto_CRYPTO_LIBRARY)
    message(STATUS "Found Crypto SSL: ${Crypto_SSL_LIBRARY}")
    message(STATUS "Found Crypto: ${Crypto_CRYPTO_LIBRARY}")
    set(Crypto_FOUND TRUE)

    if(NOT TARGET openssl::ssl::static)
        add_library(openssl::ssl::static STATIC IMPORTED)
        set_target_properties(openssl::ssl::static PROPERTIES
            IMPORTED_LOCATION "${Crypto_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Crypto_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET openssl::crypto::static)
        add_library(openssl::crypto::static STATIC IMPORTED)
        set_target_properties(openssl::crypto::static PROPERTIES
            IMPORTED_LOCATION "${Crypto_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Crypto_INCLUDE_DIR}"
        )
    endif()

    # Also provide shared targets as aliases (pointing to the same static libs)
    # to avoid link errors if shared targets are referenced
    if(NOT TARGET openssl::ssl::shared)
        add_library(openssl::ssl::shared ALIAS openssl::ssl::static)
    endif()
    if(NOT TARGET openssl::crypto::shared)
        add_library(openssl::crypto::shared ALIAS openssl::crypto::static)
    endif()
else()
    set(Crypto_FOUND FALSE)
endif()

find_package_handle_standard_args(Crypto
    REQUIRED_VARS Crypto_SSL_LIBRARY Crypto_CRYPTO_LIBRARY Crypto_INCLUDE_DIR
)

mark_as_advanced(Crypto_INCLUDE_DIR Crypto_SSL_LIBRARY Crypto_CRYPTO_LIBRARY)
