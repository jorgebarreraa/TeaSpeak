# - Find ThreadPool
#
# Usage of this module:
#
#     find_package(ThreadPool REQUIRED)
#
# Variables defined by this module:
#
#  ThreadPool_FOUND                System has ThreadPool
#  ThreadPool_INCLUDE_DIR          The ThreadPool include directory
#  threadpool::static              IMPORTED target (legacy)

include(FindPackageHandleStandardArgs)

# Get OS type and arch from environment if not set
if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE $ENV{build_os_type})
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif()

# ThreadPool is header-only, so we only need to find the include directory
# The task_executor.h is now part of TeaSpeakLibrary's misc directory
set(THREADPOOL_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/build/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/src
    ${CMAKE_SOURCE_DIR}/../shared/src
)

message(STATUS "Searching for ThreadPool in: ${THREADPOOL_SEARCH_PATHS}")

# Find include directory
find_path(ThreadPool_INCLUDE_DIR
    NAMES ThreadPool/Timer.h misc/task_executor.h
    PATHS ${THREADPOOL_SEARCH_PATHS}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT ThreadPool_INCLUDE_DIR)
    # Try system paths
    find_path(ThreadPool_INCLUDE_DIR
        NAMES ThreadPool/Timer.h misc/task_executor.h
        PATHS /usr/local/include /usr/include
    )
endif()

# ThreadPool is header-only, so no library to link
# We just need to provide the include directory

if(ThreadPool_INCLUDE_DIR)
    message(STATUS "Found ThreadPool include directory: ${ThreadPool_INCLUDE_DIR}")

    # Create an interface library target for compatibility
    if(NOT TARGET threadpool::static)
        add_library(threadpool::static INTERFACE IMPORTED)
        set_target_properties(threadpool::static PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ThreadPool_INCLUDE_DIR}"
        )
    endif()

    set(ThreadPool_FOUND TRUE)
else()
    message(STATUS "ThreadPool not found, using TeaSpeakLibrary's task_executor implementation")
    # Use TeaSpeakLibrary's task_executor.h instead
    set(ThreadPool_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../shared/src")

    if(NOT TARGET threadpool::static)
        add_library(threadpool::static INTERFACE IMPORTED)
        set_target_properties(threadpool::static PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ThreadPool_INCLUDE_DIR}"
        )
    endif()

    set(ThreadPool_FOUND TRUE)
endif()

find_package_handle_standard_args(ThreadPool
    REQUIRED_VARS ThreadPool_INCLUDE_DIR
)

mark_as_advanced(
    ThreadPool_INCLUDE_DIR
)
