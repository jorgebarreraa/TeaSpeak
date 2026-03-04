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
#  ThreadPool_LIBRARY              The ThreadPool static library (or empty if header-only fallback)
#  threadpool::static              IMPORTED target

include(FindPackageHandleStandardArgs)

# Get OS type and arch from environment if not set
if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE $ENV{build_os_type})
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif()

# Search paths for include directory
set(THREADPOOL_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/../libraries/Thread-Pool/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/build/include
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/src
    ${CMAKE_SOURCE_DIR}/shared/src
)

# Corresponding library search paths (parallel order to include paths above)
set(THREADPOOL_LIB_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/../libraries/Thread-Pool/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/lib
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/lib
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}/lib
    ${CMAKE_SOURCE_DIR}/../libraries/threadpool/build/lib
)

message(STATUS "Searching for ThreadPool in: ${THREADPOOL_SEARCH_PATHS}")

# Find include directory (ThreadPool/Timer.h = compiled Thread-Pool, misc/task_executor.h = fallback)
find_path(ThreadPool_INCLUDE_DIR
    NAMES ThreadPool/Timer.h misc/task_executor.h
    PATHS ${THREADPOOL_SEARCH_PATHS}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(NOT ThreadPool_INCLUDE_DIR)
    # Try system paths as last resort
    find_path(ThreadPool_INCLUDE_DIR
        NAMES ThreadPool/Timer.h misc/task_executor.h
        PATHS /usr/local/include /usr/include
    )
endif()

# Find the compiled static library.
# Thread-Pool (jorgebarreraa/Thread-Pool.git) is NOT header-only: threads::ThreadPool,
# threads::timer, threads::impl::ThreadBase, etc. all have compiled implementations.
# PARCHE 44 builds the library as libThreadPoolStatic.a.
find_library(ThreadPool_LIBRARY
    NAMES libThreadPoolStatic.a ThreadPoolStatic
    PATHS ${THREADPOOL_LIB_SEARCH_PATHS}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(ThreadPool_INCLUDE_DIR)
    message(STATUS "Found ThreadPool include directory: ${ThreadPool_INCLUDE_DIR}")

    if(NOT TARGET threadpool::static)
        if(ThreadPool_LIBRARY)
            message(STATUS "Found ThreadPool library: ${ThreadPool_LIBRARY}")
            # STATIC IMPORTED: links the compiled library AND exposes headers
            add_library(threadpool::static STATIC IMPORTED)
            set_target_properties(threadpool::static PROPERTIES
                IMPORTED_LOCATION "${ThreadPool_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ThreadPool_INCLUDE_DIR}"
            )
        else()
            message(STATUS "ThreadPool library not found — using headers-only INTERFACE target")
            add_library(threadpool::static INTERFACE IMPORTED)
            set_target_properties(threadpool::static PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${ThreadPool_INCLUDE_DIR}"
            )
        endif()
    endif()

    set(ThreadPool_FOUND TRUE)
else()
    message(STATUS "ThreadPool not found, using TeaSpeakLibrary's task_executor implementation")
    # Header-only fallback via TeaSpeakLibrary's shared/src
    set(ThreadPool_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/shared/src")

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
    ThreadPool_LIBRARY
)
