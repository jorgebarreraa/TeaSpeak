# - Find jemalloc memory allocator
#
# Usage:
#     find_package(Jemalloc REQUIRED)
#
# Variables defined by this module:
#
#  Jemalloc_FOUND             System has jemalloc
#  Jemalloc_INCLUDE_DIR       The include directory
#  Jemalloc_LIBRARIES         The static library

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

set(_jemalloc_search_paths
    ${jemalloc_ROOT_DIR}
    ${LIBRARY_PATH}/jemalloc/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    /usr/local
    /usr
)

find_path(Jemalloc_INCLUDE_DIR
    NAMES jemalloc/jemalloc.h
    PATHS ${_jemalloc_search_paths}
    PATH_SUFFIXES include
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

find_library(Jemalloc_LIBRARIES
    NAMES libjemalloc_pic.a libjemalloc.a jemalloc
    PATHS ${_jemalloc_search_paths}
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT Jemalloc_INCLUDE_DIR OR NOT Jemalloc_LIBRARIES)
    find_path(Jemalloc_INCLUDE_DIR NAMES jemalloc/jemalloc.h PATHS /usr/local/include /usr/include)
    find_library(Jemalloc_LIBRARIES NAMES jemalloc PATHS /usr/local/lib /usr/lib)
endif()

if(Jemalloc_INCLUDE_DIR AND Jemalloc_LIBRARIES)
    message(STATUS "Found Jemalloc: ${Jemalloc_LIBRARIES}")
    set(Jemalloc_FOUND TRUE)
    if(NOT TARGET jemalloc::static)
        add_library(jemalloc::static STATIC IMPORTED)
        set_target_properties(jemalloc::static PROPERTIES
            IMPORTED_LOCATION "${Jemalloc_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Jemalloc_INCLUDE_DIR}"
        )
    endif()
    if(NOT TARGET jemalloc::shared)
        add_library(jemalloc::shared STATIC IMPORTED)
        set_target_properties(jemalloc::shared PROPERTIES
            IMPORTED_LOCATION "${Jemalloc_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Jemalloc_INCLUDE_DIR}"
        )
    endif()
else()
    set(Jemalloc_FOUND FALSE)
    message(STATUS "Jemalloc not found in project libraries or system")
endif()

find_package_handle_standard_args(Jemalloc
    REQUIRED_VARS Jemalloc_INCLUDE_DIR Jemalloc_LIBRARIES
)

mark_as_advanced(Jemalloc_INCLUDE_DIR Jemalloc_LIBRARIES)
