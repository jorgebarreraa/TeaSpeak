# - Find Breakpad crash reporting library
#
# Usage:
#     find_package(Breakpad)
#
# Variables defined by this module:
#
#  Breakpad_FOUND             System has Breakpad
#  breakpad_INCLUDE_DIR       The include directory
#  breakpad_LIBRARY           The static library

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

set(_bp_search_paths
    ${breakpad_ROOT_DIR}
    ${LIBRARY_PATH}/breakpad/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/breakpad/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    /usr/local
    /usr
)

find_path(breakpad_INCLUDE_DIR
    NAMES client/linux/handler/exception_handler.h
    PATHS ${_bp_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(breakpad_LIBRARY
    NAMES libbreakpad_client.a breakpad_client
    PATHS ${_bp_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(breakpad_INCLUDE_DIR AND breakpad_LIBRARY)
    message(STATUS "Found Breakpad: ${breakpad_LIBRARY}")
    set(Breakpad_FOUND TRUE)
    if(NOT TARGET breakpad::static)
        add_library(breakpad::static STATIC IMPORTED)
        set_target_properties(breakpad::static PROPERTIES
            IMPORTED_LOCATION "${breakpad_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${breakpad_INCLUDE_DIR}"
        )
    endif()
else()
    set(Breakpad_FOUND FALSE)
    if(Breakpad_FIND_REQUIRED)
        message(FATAL_ERROR "Breakpad not found")
    else()
        message(STATUS "Breakpad not found (optional - crash reporting disabled)")
    endif()
endif()

find_package_handle_standard_args(Breakpad
    REQUIRED_VARS breakpad_INCLUDE_DIR breakpad_LIBRARY
)

mark_as_advanced(breakpad_INCLUDE_DIR breakpad_LIBRARY)
