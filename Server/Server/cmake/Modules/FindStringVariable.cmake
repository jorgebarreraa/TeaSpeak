# - Find StringVariable
#
# Usage:
#     find_package(StringVariable REQUIRED)
#
# Variables defined by this module:
#
#  StringVariable_FOUND                 System has StringVariable
#  StringVariable_INCLUDE_DIR           The include directory
#  StringVariable_LIBRARIES_STATIC      The static library

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

set(_sv_search_paths
    ${StringVariable_ROOT_DIR}
    ${LIBRARY_PATH}/StringVariable/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/StringVariable/_cmake_build
    /usr/local
    /usr
)

find_path(StringVariable_INCLUDE_DIR
    NAMES StringVariable.h
    PATHS ${_sv_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(StringVariable_LIBRARIES_STATIC
    NAMES libStringVariablesStatic.a StringVariablesStatic StringVariable
    PATHS ${_sv_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT StringVariable_INCLUDE_DIR OR NOT StringVariable_LIBRARIES_STATIC)
    find_path(StringVariable_INCLUDE_DIR NAMES StringVariable.h PATHS /usr/local/include /usr/include)
    find_library(StringVariable_LIBRARIES_STATIC NAMES StringVariablesStatic StringVariable PATHS /usr/local/lib /usr/lib)
endif()

if(StringVariable_INCLUDE_DIR AND StringVariable_LIBRARIES_STATIC)
    message(STATUS "Found StringVariable: ${StringVariable_LIBRARIES_STATIC}")
endif()

find_package_handle_standard_args(StringVariable
    REQUIRED_VARS StringVariable_INCLUDE_DIR StringVariable_LIBRARIES_STATIC
)

mark_as_advanced(StringVariable_INCLUDE_DIR StringVariable_LIBRARIES_STATIC)
