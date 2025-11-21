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

# Get OS type and arch from environment if not set
if(NOT BUILD_OS_TYPE)
    set(BUILD_OS_TYPE $ENV{build_os_type})
endif()
if(NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif()

# Search for LibeventConfig.cmake in project libraries FIRST
set(LIBEVENT_CONFIG_PATHS
    ${LIBRARY_PATH}/event/_build/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/event/out/${BUILD_OS_TYPE}_${BUILD_OS_ARCH}
    ${LIBRARY_PATH}/event/build
)

message(STATUS "BUILD_OS_TYPE=${BUILD_OS_TYPE}, BUILD_OS_ARCH=${BUILD_OS_ARCH}")
message(STATUS "Searching for LibeventConfig.cmake in: ${LIBEVENT_CONFIG_PATHS}")

# Try to find the LibeventConfig.cmake file in project directories
find_path(LIBEVENT_CONFIG_DIR
    NAMES LibeventConfig.cmake
    PATHS ${LIBEVENT_CONFIG_PATHS}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
)

if(LIBEVENT_CONFIG_DIR)
    message(STATUS "Found LibeventConfig.cmake in: ${LIBEVENT_CONFIG_DIR}")

    # Include the project's LibeventConfig.cmake
    include(${LIBEVENT_CONFIG_DIR}/LibeventConfig.cmake)

    # The config file defines LIBEVENT_INCLUDE_DIRS
    message(STATUS "Libevent include dirs: ${LIBEVENT_INCLUDE_DIRS}")

    # Try to determine version from event-config.h
    set(Libevent_VERSION "2.2.0")
    foreach(INCLUDE_DIR ${LIBEVENT_INCLUDE_DIRS})
        if(EXISTS "${INCLUDE_DIR}/event2/event-config.h")
            file(READ "${INCLUDE_DIR}/event2/event-config.h" EVENT_CONFIG_H)

            if(EVENT_CONFIG_H MATCHES "#define _EVENT_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)")
                set(Libevent_VERSION ${CMAKE_MATCH_1})
                break()
            elseif(EVENT_CONFIG_H MATCHES "#define EVENT__VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)")
                set(Libevent_VERSION ${CMAKE_MATCH_1})
                break()
            elseif(EVENT_CONFIG_H MATCHES "EVENT__NUMERIC_VERSION 0x([0-9a-f]+)")
                set(NUMERIC_VERSION ${CMAKE_MATCH_1})
                math(EXPR VERSION_MAJOR "0x${NUMERIC_VERSION} / 0x1000000")
                math(EXPR VERSION_MINOR "(0x${NUMERIC_VERSION} / 0x10000) % 0x100")
                math(EXPR VERSION_PATCH "(0x${NUMERIC_VERSION} / 0x100) % 0x100")
                set(Libevent_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
                break()
            endif()
        endif()
    endforeach()

    message(STATUS "Detected libevent version: ${Libevent_VERSION}")

    # Create namespace targets by copying properties from existing IMPORTED targets
    # The LibeventTargets.cmake defines: event_core_static, event_pthreads_static, etc.
    # We need to create: libevent::core, libevent::pthreads
    # Cannot use ALIAS because imported targets are not globally visible

    if(LIBEVENT_STATIC_LINK)
        # Use static libraries
        if(TARGET event_core_static AND NOT TARGET libevent::core)
            get_target_property(_LOCATION event_core_static IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_core_static IMPORTED_LOCATION)
            endif()
            add_library(libevent::core STATIC IMPORTED)
            set_target_properties(libevent::core PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
            )
            message(STATUS "Created libevent::core from ${_LOCATION}")
        endif()

        if(TARGET event_pthreads_static AND NOT TARGET libevent::pthreads)
            get_target_property(_LOCATION event_pthreads_static IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_pthreads_static IMPORTED_LOCATION)
            endif()
            add_library(libevent::pthreads STATIC IMPORTED)
            set_target_properties(libevent::pthreads PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "libevent::core;pthread"
            )
            message(STATUS "Created libevent::pthreads from ${_LOCATION}")
        endif()

        if(TARGET event_extra_static AND NOT TARGET libevent::extra)
            get_target_property(_LOCATION event_extra_static IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_extra_static IMPORTED_LOCATION)
            endif()
            add_library(libevent::extra STATIC IMPORTED)
            set_target_properties(libevent::extra PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "libevent::core"
            )
        endif()
    else()
        # Use shared libraries
        if(TARGET event_core_shared AND NOT TARGET libevent::core)
            get_target_property(_LOCATION event_core_shared IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_core_shared IMPORTED_LOCATION)
            endif()
            get_target_property(_SONAME event_core_shared IMPORTED_SONAME_RELEASE)
            add_library(libevent::core SHARED IMPORTED)
            set_target_properties(libevent::core PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                IMPORTED_SONAME "${_SONAME}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "pthread"
            )
            message(STATUS "Created libevent::core from ${_LOCATION}")
        endif()

        if(TARGET event_pthreads_shared AND NOT TARGET libevent::pthreads)
            get_target_property(_LOCATION event_pthreads_shared IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_pthreads_shared IMPORTED_LOCATION)
            endif()
            get_target_property(_SONAME event_pthreads_shared IMPORTED_SONAME_RELEASE)
            add_library(libevent::pthreads SHARED IMPORTED)
            set_target_properties(libevent::pthreads PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                IMPORTED_SONAME "${_SONAME}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "libevent::core;pthread"
            )
            message(STATUS "Created libevent::pthreads from ${_LOCATION}")
        endif()

        if(TARGET event_extra_shared AND NOT TARGET libevent::extra)
            get_target_property(_LOCATION event_extra_shared IMPORTED_LOCATION_RELEASE)
            if(NOT _LOCATION)
                get_target_property(_LOCATION event_extra_shared IMPORTED_LOCATION)
            endif()
            get_target_property(_SONAME event_extra_shared IMPORTED_SONAME_RELEASE)
            add_library(libevent::extra SHARED IMPORTED)
            set_target_properties(libevent::extra PROPERTIES
                IMPORTED_LOCATION "${_LOCATION}"
                IMPORTED_SONAME "${_SONAME}"
                INTERFACE_INCLUDE_DIRECTORIES "${LIBEVENT_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "libevent::core"
            )
        endif()
    endif()

    # Extract major.minor for version check
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)" _ "${Libevent_VERSION}")
    set(Libevent_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(Libevent_VERSION_MINOR ${CMAKE_MATCH_2})
    set(Libevent_FOUND_VERSION "${Libevent_VERSION_MAJOR}.${Libevent_VERSION_MINOR}")

    # Mark as found
    set(Libevent_FOUND TRUE)

    find_package_handle_standard_args(Libevent
        REQUIRED_VARS LIBEVENT_INCLUDE_DIRS
        VERSION_VAR Libevent_FOUND_VERSION
    )
else()
    message(STATUS "LibeventConfig.cmake not found in project, searching system paths")
    set(Libevent_FOUND FALSE)
endif()
