# - Find CXXTerminal
#
# Usage:
#     find_package(CXXTerminal)
#
# Variables defined by this module:
#
#  CXXTerminal_FOUND            System has CXXTerminal
#  CXXTerminal_INCLUDE_DIR      The include directory
#  CXXTerminal::static          IMPORTED library target (STATIC or SHARED depending on what's found)

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
        # Detect whether the found library is shared (.so) or static (.a).
        # CMake only adds a library's directory to the binary's rpath for
        # SHARED IMPORTED targets. Creating a STATIC IMPORTED target for a .so
        # causes the linker to find it at build time but the dynamic linker
        # can't find it at runtime (no rpath entry). Detect the actual type and
        # create the correct IMPORTED target so cmake adds rpath automatically.
        if(CXXTerminal_LIBRARY MATCHES "\\.so(\\.[0-9]+)*$")
            add_library(CXXTerminal::static SHARED IMPORTED)
        else()
            add_library(CXXTerminal::static STATIC IMPORTED)
        endif()
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
