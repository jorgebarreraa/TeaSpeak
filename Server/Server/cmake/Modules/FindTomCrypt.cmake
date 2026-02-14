# - Find TomCrypt
#
# Usage:
#     find_package(TomCrypt REQUIRED)
#
# Variables defined by this module:
#
#  TomCrypt_FOUND              System has TomCrypt
#  TomCrypt_INCLUDE_DIR        The TomCrypt include directory
#  TomCrypt_LIBRARIES          The TomCrypt static library

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

set(_tomcrypt_search_paths
    ${TomCrypt_ROOT_DIR}
    ${LIBRARY_PATH}/tomcrypt/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    /usr/local
    /usr
)

find_path(TomCrypt_INCLUDE_DIR
    NAMES tomcrypt.h
    PATHS ${_tomcrypt_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(TomCrypt_LIBRARIES
    NAMES libtomcrypt.a tomcrypt
    PATHS ${_tomcrypt_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT TomCrypt_INCLUDE_DIR OR NOT TomCrypt_LIBRARIES)
    find_path(TomCrypt_INCLUDE_DIR NAMES tomcrypt.h)
    find_library(TomCrypt_LIBRARIES NAMES tomcrypt)
endif()

if(TomCrypt_INCLUDE_DIR AND TomCrypt_LIBRARIES)
    message(STATUS "Found TomCrypt: ${TomCrypt_LIBRARIES}")
    if(NOT TARGET TomCrypt::static)
        add_library(TomCrypt::static STATIC IMPORTED)
        set_target_properties(TomCrypt::static PROPERTIES
            IMPORTED_LOCATION "${TomCrypt_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${TomCrypt_INCLUDE_DIR}"
        )
    endif()
endif()

find_package_handle_standard_args(TomCrypt
    REQUIRED_VARS TomCrypt_INCLUDE_DIR TomCrypt_LIBRARIES
)

mark_as_advanced(TomCrypt_INCLUDE_DIR TomCrypt_LIBRARIES)
