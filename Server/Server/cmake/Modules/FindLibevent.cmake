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
    # Get OS type and arch from environment if not set
    if(NOT BUILD_OS_TYPE)
        set(BUILD_OS_TYPE $ENV{build_os_type})
    endif()
    if(NOT BUILD_OS_ARCH)
        set(BUILD_OS_ARCH $ENV{build_os_arch})
    endif()

    # First, try to find in project libraries (NO system paths)
    set(LIBEVENT_PROJECT_PATHS
        ${LIBRARY_PATH}/event/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
        ${LIBRARY_PATH}/event/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
        ${LIBRARY_PATH}/event/build
    )

    message(STATUS "BUILD_OS_TYPE=${BUILD_OS_TYPE}, BUILD_OS_ARCH=${BUILD_OS_ARCH}")
    message(STATUS "Searching for libevent in project paths: ${LIBEVENT_PROJECT_PATHS}")

    # Find the root directory in project FIRST
    find_path(Libevent_ROOT_DIR
        NAMES include/event2/event.h
        PATHS ${LIBEVENT_PROJECT_PATHS}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )

    # Only if not found in project, search system paths
    if(NOT Libevent_ROOT_DIR)
        message(STATUS "Libevent not found in project, searching system paths")
        find_path(Libevent_ROOT_DIR
            NAMES include/event2/event.h
            PATHS /usr/local /usr
        )
    else()
        message(STATUS "Found libevent root: ${Libevent_ROOT_DIR}")
    endif()

    # Find include directory
    find_path(LIBEVENT_INCLUDE_DIRS
        NAMES event2/event.h
        PATHS ${Libevent_ROOT_DIR}/include
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )

    if(NOT LIBEVENT_INCLUDE_DIRS)
        find_path(LIBEVENT_INCLUDE_DIRS
            NAMES event2/event.h
            PATHS /usr/local/include /usr/include
        )
    endif()

    # Determine library suffix based on static link preference
    if(LIBEVENT_STATIC_LINK)
        set(LIB_NAMES libevent_core.a event_core)
        set(LIB_NAMES_PTHREADS libevent_pthreads.a event_pthreads)
        message(STATUS "Searching for STATIC libevent libraries")
    else()
        set(LIB_NAMES event_core libevent_core.so)
        set(LIB_NAMES_PTHREADS event_pthreads libevent_pthreads.so)
    endif()

    # Find core library - project paths ONLY first
    find_library(LIBEVENT_CORE_LIBRARY
        NAMES ${LIB_NAMES}
        PATHS ${Libevent_ROOT_DIR}/lib ${Libevent_ROOT_DIR}/.libs
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )

    if(NOT LIBEVENT_CORE_LIBRARY)
        find_library(LIBEVENT_CORE_LIBRARY
            NAMES event_core
            PATHS /usr/local/lib /usr/lib
        )
    endif()

    # Find pthreads library - project paths ONLY first
    find_library(LIBEVENT_PTHREADS_LIBRARY
        NAMES ${LIB_NAMES_PTHREADS}
        PATHS ${Libevent_ROOT_DIR}/lib ${Libevent_ROOT_DIR}/.libs
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )

    if(NOT LIBEVENT_PTHREADS_LIBRARY)
        find_library(LIBEVENT_PTHREADS_LIBRARY
            NAMES event_pthreads
            PATHS /usr/local/lib /usr/lib
        )
    endif()

    # Try to determine version
    if(LIBEVENT_INCLUDE_DIRS AND EXISTS "${LIBEVENT_INCLUDE_DIRS}/event2/event-config.h")
        file(READ "${LIBEVENT_INCLUDE_DIRS}/event2/event-config.h" EVENT_CONFIG_H)

        # Try different version patterns
        if(EVENT_CONFIG_H MATCHES "#define _EVENT_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(Libevent_VERSION ${CMAKE_MATCH_1})
        elseif(EVENT_CONFIG_H MATCHES "#define EVENT__VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(Libevent_VERSION ${CMAKE_MATCH_1})
        elseif(EVENT_CONFIG_H MATCHES "EVENT__NUMERIC_VERSION 0x([0-9a-f]+)")
            # Parse numeric version (e.g., 0x02020100 = 2.2.1)
            set(NUMERIC_VERSION ${CMAKE_MATCH_1})
            math(EXPR VERSION_MAJOR "0x${NUMERIC_VERSION} / 0x1000000")
            math(EXPR VERSION_MINOR "(0x${NUMERIC_VERSION} / 0x10000) % 0x100")
            math(EXPR VERSION_PATCH "(0x${NUMERIC_VERSION} / 0x100) % 0x100")
            set(Libevent_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
        endif()

        if(Libevent_VERSION)
            # Extract major.minor for version check
            string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _ "${Libevent_VERSION}")
            set(Libevent_VERSION_MAJOR ${CMAKE_MATCH_1})
            set(Libevent_VERSION_MINOR ${CMAKE_MATCH_2})
            set(Libevent_FOUND_VERSION "${Libevent_VERSION_MAJOR}.${Libevent_VERSION_MINOR}")
            message(STATUS "Detected libevent version: ${Libevent_VERSION}")
        else()
            # If we can't determine version but found the library, assume it's compatible
            message(WARNING "Could not determine libevent version from headers, assuming 2.2+")
            set(Libevent_FOUND_VERSION "2.2")
        endif()
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
