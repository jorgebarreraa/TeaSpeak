# - Find CXXTerminal
#
# Usage:
#     find_package(CXXTerminal)
#
# Variables defined by this module:
#
#  CXXTerminal_FOUND            System has CXXTerminal
#  CXXTerminal_INCLUDE_DIR      The include directory
#  CXXTerminal::static          IMPORTED static library target

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

set(_cxxt_search_paths
    ${CXXTerminal_ROOT_DIR}
    ${LIBRARY_PATH}/CXXTerminal/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/CXXTerminal/_cmake_build
)

find_path(CXXTerminal_INCLUDE_DIR
    NAMES CXXTerminal/QuickTerminal.h
    PATHS ${_cxxt_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(CXXTerminal_LIBRARY
    NAMES libCXXTerminal.a CXXTerminal
    PATHS ${_cxxt_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(CXXTerminal_INCLUDE_DIR AND CXXTerminal_LIBRARY)
    message(STATUS "Found CXXTerminal: ${CXXTerminal_LIBRARY}")
    set(CXXTerminal_FOUND TRUE)

    if(NOT TARGET CXXTerminal::static)
        add_library(CXXTerminal::static STATIC IMPORTED)
        set_target_properties(CXXTerminal::static PROPERTIES
            IMPORTED_LOCATION "${CXXTerminal_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CXXTerminal_INCLUDE_DIR}"
        )
    endif()
else()
    set(CXXTerminal_FOUND FALSE)
    if(CXXTerminal_FIND_REQUIRED)
        message(FATAL_ERROR "CXXTerminal not found")
    else()
        message(STATUS "CXXTerminal not found (optional - terminal features disabled)")
    endif()
endif()

mark_as_advanced(CXXTerminal_INCLUDE_DIR CXXTerminal_LIBRARY)
