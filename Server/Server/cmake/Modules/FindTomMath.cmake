# - Find TomMath
#
# Usage:
#     find_package(TomMath REQUIRED)
#
# Variables defined by this module:
#
#  TomMath_FOUND              System has TomMath
#  TomMath_INCLUDE_DIR        The TomMath include directory
#  TomMath_LIBRARIES          The TomMath static library
#  tommath::static            IMPORTED target

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

set(_tommath_search_paths
    ${TomMath_ROOT_DIR}
    ${LIBRARY_PATH}/tommath/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/tommath/_cmake_build
    /usr/local
    /usr
)

find_path(TomMath_INCLUDE_DIR
    NAMES tommath.h
    PATHS ${_tommath_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(TomMath_LIBRARIES
    NAMES libtommathStatic.a tommathStatic libtommath.a tommath
    PATHS ${_tommath_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT TomMath_INCLUDE_DIR OR NOT TomMath_LIBRARIES)
    find_path(TomMath_INCLUDE_DIR NAMES tommath.h)
    find_library(TomMath_LIBRARIES NAMES tommath)
endif()

if(TomMath_INCLUDE_DIR AND TomMath_LIBRARIES)
    message(STATUS "Found TomMath: ${TomMath_LIBRARIES}")
    if(NOT TARGET tommath::static)
        add_library(tommath::static STATIC IMPORTED)
        set_target_properties(tommath::static PROPERTIES
            IMPORTED_LOCATION "${TomMath_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${TomMath_INCLUDE_DIR}"
        )
    endif()
endif()

find_package_handle_standard_args(TomMath
    REQUIRED_VARS TomMath_INCLUDE_DIR TomMath_LIBRARIES
)

mark_as_advanced(TomMath_INCLUDE_DIR TomMath_LIBRARIES)
