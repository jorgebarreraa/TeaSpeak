# - Try to find Libevent include dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(Libevent 2.2 REQUIRED COMPONENTS core pthreads)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Libevent_ROOT_DIR        Set this variable to the root installation of
#                           Libevent if the module has problems finding the
#                           proper installation path.
#  LIBEVENT_STATIC_LINK     Set to TRUE to prefer static libraries
#
# Variables defined by this module:
#
#  Libevent_FOUND           System has Libevent, include and library dirs found
#  LIBEVENT_INCLUDE_DIRS    The Libevent include directories.
#  Libevent_VERSION         The Libevent version

include(FindPackageHandleStandardArgs)

# Function to find libevent
function(find_libevent)
    set(LIBEVENT_SEARCH_PATHS
        ${LIBRARY_PATH}/event/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
        ${LIBRARY_PATH}/event/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
        ${LIBRARY_PATH}/event/build
        /usr/local
        /usr
    )

    # Find the root directory
    find_path(Libevent_ROOT_DIR
        NAMES include/event2/event.h
        PATHS ${LIBEVENT_SEARCH_PATHS}
        NO_DEFAULT_PATH
    )

    # Also search system paths if not found
    if(NOT Libevent_ROOT_DIR)
        find_path(Libevent_ROOT_DIR
            NAMES include/event2/event.h
            PATHS /usr/local /usr
        )
    endif()

    # Find include directory
    find_path(LIBEVENT_INCLUDE_DIRS
        NAMES event2/event.h
        PATHS ${Libevent_ROOT_DIR}/include
        NO_DEFAULT_PATH
    )

    if(NOT LIBEVENT_INCLUDE_DIRS)
        find_path(LIBEVENT_INCLUDE_DIRS
            NAMES event2/event.h
            PATHS /usr/local/include /usr/include
        )
    endif()

    # Determine library suffix based on static link preference
    if(LIBEVENT_STATIC_LINK)
        set(LIB_SUFFIX ".a")
    else()
        set(LIB_SUFFIX ".so")
    endif()

    # Find core library
    find_library(LIBEVENT_CORE_LIBRARY
        NAMES libevent_core${LIB_SUFFIX} event_core
        PATHS ${Libevent_ROOT_DIR}/lib ${Libevent_ROOT_DIR}/.libs
        NO_DEFAULT_PATH
    )

    if(NOT LIBEVENT_CORE_LIBRARY)
        find_library(LIBEVENT_CORE_LIBRARY
            NAMES event_core
            PATHS /usr/local/lib /usr/lib
        )
    endif()

    # Find pthreads library
    find_library(LIBEVENT_PTHREADS_LIBRARY
        NAMES libevent_pthreads${LIB_SUFFIX} event_pthreads
        PATHS ${Libevent_ROOT_DIR}/lib ${Libevent_ROOT_DIR}/.libs
        NO_DEFAULT_PATH
    )

    if(NOT LIBEVENT_PTHREADS_LIBRARY)
        find_library(LIBEVENT_PTHREADS_LIBRARY
            NAMES event_pthreads
            PATHS /usr/local/lib /usr/lib
        )
    endif()

    # Try to determine version
    if(LIBEVENT_INCLUDE_DIRS)
        file(READ "${LIBEVENT_INCLUDE_DIRS}/event2/event-config.h" EVENT_CONFIG_H)
        string(REGEX MATCH "#define _EVENT_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)" _ ${EVENT_CONFIG_H})
        set(Libevent_VERSION ${CMAKE_MATCH_1})

        # Extract major.minor for version check
        string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _ ${Libevent_VERSION})
        set(Libevent_VERSION_MAJOR ${CMAKE_MATCH_1})
        set(Libevent_VERSION_MINOR ${CMAKE_MATCH_2})
        set(Libevent_FOUND_VERSION "${Libevent_VERSION_MAJOR}.${Libevent_VERSION_MINOR}")
    endif()

    # Create imported target for libevent::core
    if(LIBEVENT_CORE_LIBRARY AND NOT TARGET libevent::core)
        message(STATUS "Creating libevent::core target from ${LIBEVENT_CORE_LIBRARY}")
        add_library(libevent::core UNKNOWN IMPORTED)
        set_target_properties(libevent::core PROPERTIES
            IMPORTED_LOCATION ${LIBEVENT_CORE_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBEVENT_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES "pthread"
        )
    endif()

    # Create imported target for libevent::pthreads
    if(LIBEVENT_PTHREADS_LIBRARY AND NOT TARGET libevent::pthreads)
        message(STATUS "Creating libevent::pthreads target from ${LIBEVENT_PTHREADS_LIBRARY}")
        add_library(libevent::pthreads UNKNOWN IMPORTED)
        set_target_properties(libevent::pthreads PROPERTIES
            IMPORTED_LOCATION ${LIBEVENT_PTHREADS_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES ${LIBEVENT_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES "pthread;libevent::core"
        )
    endif()

    # Set Libevent_FOUND variable
    find_package_handle_standard_args(Libevent
        REQUIRED_VARS LIBEVENT_CORE_LIBRARY LIBEVENT_PTHREADS_LIBRARY LIBEVENT_INCLUDE_DIRS
        VERSION_VAR Libevent_FOUND_VERSION
    )

    mark_as_advanced(
        Libevent_ROOT_DIR
        LIBEVENT_INCLUDE_DIRS
        LIBEVENT_CORE_LIBRARY
        LIBEVENT_PTHREADS_LIBRARY
    )
endfunction()

find_libevent()
