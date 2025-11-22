# - Try to find Ed25519 include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(Ed25519 REQUIRED)
#
# Variables defined by this module:
#
#  Ed25519_FOUND              System has Ed25519
#  ed25519_INCLUDE_DIR        The Ed25519 include directory
#  ed25519_LIBRARIES_STATIC   The Ed25519 static library

include(FindPackageHandleStandardArgs)

# Get OS type and arch from environment if not set
if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE $ENV{build_os_type})
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif()

# Search for Ed25519 in project libraries
set(ED25519_SEARCH_PATHS
    ${LIBRARY_PATH}/ed25519/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/ed25519/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/ed25519/build
)

message(STATUS "Searching for Ed25519 in: ${ED25519_SEARCH_PATHS}")

# Find include directory
find_path(ed25519_INCLUDE_DIR
    NAMES ed25519/ed25519.h
    PATHS ${ED25519_SEARCH_PATHS}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT ed25519_INCLUDE_DIR)
    find_path(ed25519_INCLUDE_DIR
        NAMES ed25519/ed25519.h
        PATHS /usr/local/include /usr/include
    )
endif()

# Find static library
find_library(ed25519_LIBRARIES_STATIC
    NAMES libed25519.a ed25519
    PATHS ${ED25519_SEARCH_PATHS}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT ed25519_LIBRARIES_STATIC)
    find_library(ed25519_LIBRARIES_STATIC
        NAMES ed25519
        PATHS /usr/local/lib /usr/lib
    )
endif()

if(ed25519_INCLUDE_DIR AND ed25519_LIBRARIES_STATIC)
    message(STATUS "Found Ed25519:")
    message(STATUS "  Include: ${ed25519_INCLUDE_DIR}")
    message(STATUS "  Library: ${ed25519_LIBRARIES_STATIC}")
endif()

find_package_handle_standard_args(Ed25519
    REQUIRED_VARS ed25519_LIBRARIES_STATIC ed25519_INCLUDE_DIR
)

mark_as_advanced(
    ed25519_INCLUDE_DIR
    ed25519_LIBRARIES_STATIC
)
