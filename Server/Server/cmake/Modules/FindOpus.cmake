# - Find Opus codec library
#
# Usage:
#     find_package(Opus)
#
# Variables defined by this module:
#
#  Opus_FOUND             System has Opus
#  Opus_INCLUDE_DIR       The include directory
#  Opus_LIBRARIES         The static library

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

set(_opus_search_paths
    ${Opus_ROOT_DIR}
    ${LIBRARY_PATH}/opus/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/opus/_cmake_build
    /usr/local
    /usr
)

find_path(Opus_INCLUDE_DIR
    NAMES opus/opus.h
    PATHS ${_opus_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(Opus_LIBRARIES
    NAMES libopus.a opus
    PATHS ${_opus_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT Opus_INCLUDE_DIR OR NOT Opus_LIBRARIES)
    find_path(Opus_INCLUDE_DIR NAMES opus/opus.h)
    find_library(Opus_LIBRARIES NAMES opus)
endif()

if(Opus_INCLUDE_DIR AND Opus_LIBRARIES)
    message(STATUS "Found Opus: ${Opus_LIBRARIES}")
    set(Opus_FOUND TRUE)
else()
    set(Opus_FOUND FALSE)
    if(Opus_FIND_REQUIRED)
        message(FATAL_ERROR "Opus not found")
    else()
        message(STATUS "Opus not found (optional)")
    endif()
endif()

find_package_handle_standard_args(Opus
    REQUIRED_VARS Opus_INCLUDE_DIR Opus_LIBRARIES
)

mark_as_advanced(Opus_INCLUDE_DIR Opus_LIBRARIES)
