cmake_minimum_required(VERSION 3.6)
project(TeaSpeak-Parent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules")
set(TEASPEAK_SERVER ON)

#end now
#set(MEMORY_DEBUG_FLAGS " -fsanitize=leak -fsanitize=address -fstack-protector-all ")
#set(MEMORY_DEBUG_FLAGS "-fsanitize=address -fstack-protector-all")
#set(MEMORY_DEBUG_FLAGS "-fstack-protector-all")
#set(MEMORY_DEBUG_FLAGS " -fsanitize=address -static-libasan")

if (NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif ()

if (NOT BUILD_OS_ARCH)
    set(BUILD_OS_ARCH $ENV{build_os_arch})
endif ()

# Use relative path to libraries directory (works from any location)
get_filename_component(LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libraries/" ABSOLUTE)
if (BUILD_INCLUDE_FILE)
    include(${BUILD_INCLUDE_FILE})
endif ()

set(LIBEVENT_PATH "${LIBRARY_PATH}/event/_build/linux_amd64/lib")

function(resolve_library VARIABLE FALLBACK PATHS)
    set( _PATHS ${PATHS} ${ARGN} ) # Merge them together

    foreach(PATH IN ITEMS ${_PATHS})
        message(STATUS "Try to use path ${PATH} for ${VARIABLE}")
        if(EXISTS ${PATH})
            message(STATUS "Setting ${VARIABLE} to ${PATH}")
            set(${VARIABLE} ${PATH} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    if(FALLBACK)
        message(WARNING "Failed to resolve library path for ${VARIABLE}. Using default ${VARIABLE}")
    else()
        message(FATAL_ERROR "Failed to find requited library. Variable: ${VARIABLE} Paths: ${_PATHS}")
    endif()
endfunction()

find_package(TomMath REQUIRED)
find_package(TomCrypt REQUIRED)
find_package(Breakpad) # Optional: crash reporting
find_package(Protobuf REQUIRED)
find_package(Crypto REQUIRED)
find_package(ThreadPool REQUIRED)
find_package(CXXTerminal) # Optional: Terminal library
find_package(StringVariable REQUIRED)
find_package(yaml-cpp REQUIRED)
# jsoncpp_lib target is manually defined in tearoot-server.cmake, skip find_package
# to avoid CMake target conflict
if(NOT TARGET jsoncpp_lib)
    find_package(jsoncpp REQUIRED)
endif()
find_package(DataPipes) # Optional: Pipes library
find_package(Opus) # Optional: Opus codec
find_package(spdlog REQUIRED)
find_package(Jemalloc REQUIRED)
# find_package(Protobuf REQUIRED) # Duplicate - already called on line 53
message("${zstd_DIR}")
find_package(zstd) # Optional: zstd compression

include_directories(${StringVariable_INCLUDE_DIR})
# Get spdlog include directory from the target
get_target_property(SPDLOG_INCLUDE_DIRS spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
include_directories(${SPDLOG_INCLUDE_DIRS})
add_subdirectory(music/)

# Add music include directory for server to access MusicPlayer.h
include_directories(music/include)

# LibEvent fucks up the CMAKE_FIND_LIBRARY_SUFFIXES variable
macro(find_event static)
    set(_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if (${static})
        set(LIBEVENT_STATIC_LINK TRUE)
        message("Use static libevent")
    endif ()
    find_package(Libevent 2.1 REQUIRED COMPONENTS core pthreads)
    if (NOT Libevent_FOUND)
        message(FATAL_ERROR "Failed to find libevent (Variable: ${LIBEVENT_FOUND})")
    endif ()
    message("Libevent version: ${Libevent_FOUND}")
endmacro()
find_event(TRUE)

check_include_file(mysql/mysql.h HAVE_MYSQL_MYSQL_H)
if(HAVE_MYSQL_MYSQL_H)
    add_definitions(-DHAVE_MYSQL_MYSQL_H)
endif()

check_include_file(mysql.h HAVE_MYSQL_H)
if(HAVE_MYSQL_H)
    add_definitions(-DHAVE_MYSQL_H)
endif()

#FIXME: Use module for this
if(breakpad_INCLUDE_DIR)
    include_directories(${breakpad_INCLUDE_DIR})
endif()
include_directories(${ThreadPool_INCLUDE_DIR})
if(DataPipes_INCLUDE_DIR)
    include_directories(${DataPipes_INCLUDE_DIR})
endif()
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${StringVariable_INCLUDE_DIR})

add_definitions(-DINET -DINET6)

# shared/ is a library directory used by other modules, not a CMake project
add_subdirectory(shared/)
add_subdirectory(server/)
add_subdirectory(license/)
add_subdirectory(MusicBot/)
add_subdirectory(file/)
