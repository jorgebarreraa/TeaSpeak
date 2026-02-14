# - Find DataPipes
#
# Usage:
#     find_package(DataPipes)
#
# Variables defined by this module:
#
#  DataPipes_FOUND                System has DataPipes
#  DataPipes_INCLUDE_DIR          The include directory
#  DataPipes::core::static        IMPORTED static library target
#  DataPipes::core::shared        IMPORTED shared library target

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

set(_dp_search_paths
    ${DataPipes_ROOT_DIR}
    ${LIBRARY_PATH}/DataPipes/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/DataPipes/_cmake_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
)

find_path(DataPipes_INCLUDE_DIR
    NAMES pipes/buffer.h
    PATHS ${_dp_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(DataPipes_LIBRARY_STATIC
    NAMES libDataPipes-Core-Static.a DataPipes-Core-Static
    PATHS ${_dp_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(DataPipes_LIBRARY_SHARED
    NAMES libDataPipes-Core-Shared.so DataPipes-Core-Shared
    PATHS ${_dp_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(DataPipes_INCLUDE_DIR AND DataPipes_LIBRARY_STATIC)
    message(STATUS "Found DataPipes: ${DataPipes_LIBRARY_STATIC}")
    set(DataPipes_FOUND TRUE)

    if(NOT TARGET DataPipes::core::static)
        add_library(DataPipes::core::static STATIC IMPORTED)
        set_target_properties(DataPipes::core::static PROPERTIES
            IMPORTED_LOCATION "${DataPipes_LIBRARY_STATIC}"
            INTERFACE_INCLUDE_DIRECTORIES "${DataPipes_INCLUDE_DIR}"
        )
    endif()

    if(DataPipes_LIBRARY_SHARED AND NOT TARGET DataPipes::core::shared)
        add_library(DataPipes::core::shared SHARED IMPORTED)
        set_target_properties(DataPipes::core::shared PROPERTIES
            IMPORTED_LOCATION "${DataPipes_LIBRARY_SHARED}"
            INTERFACE_INCLUDE_DIRECTORIES "${DataPipes_INCLUDE_DIR}"
        )
    endif()
else()
    set(DataPipes_FOUND FALSE)
    if(DataPipes_FIND_REQUIRED)
        message(FATAL_ERROR "DataPipes not found")
    else()
        message(STATUS "DataPipes not found (optional)")
    endif()
endif()

mark_as_advanced(DataPipes_INCLUDE_DIR DataPipes_LIBRARY_STATIC DataPipes_LIBRARY_SHARED)
