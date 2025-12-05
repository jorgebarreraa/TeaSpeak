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

set(LIBRARY_PATH "${CMAKE_SOURCE_DIR}/../libraries/")
if (BUILD_INCLUDE_FILE)
    include(${BUILD_INCLUDE_FILE})
endif ()

set(LIBEVENT_PATH "${LIBRARY_PATH}/event/build/lib/")

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
find_package(Breakpad REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Crypto REQUIRED)
find_package(ThreadPool REQUIRED)
find_package(CXXTerminal REQUIRED)
find_package(StringVariable REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(DataPipes REQUIRED)
find_package(Opus REQUIRED)
find_package(spdlog REQUIRED)
find_package(Jemalloc) # Made optional - not required for compilation
find_package(Protobuf REQUIRED)
message("${zstd_DIR}")
find_package(zstd REQUIRED)

include_directories(${StringVariable_INCLUDE_DIR})
add_subdirectory(music/)

# LibEvent fucks up the CMAKE_FIND_LIBRARY_SUFFIXES variable
macro(find_event static)
    set(_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    if (${static})
        set(LIBEVENT_STATIC_LINK TRUE)
        message("Use static libevent")
    endif ()
    find_package(Libevent 2.2 REQUIRED COMPONENTS core pthreads)
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
include_directories(${breakpad_INCLUDE_DIR})
include_directories(${ThreadPool_INCLUDE_DIR})
include_directories(${DataPipes_INCLUDE_DIR})
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${StringVariable_INCLUDE_DIR})

add_definitions(-DINET -DINET6)

add_subdirectory(shared/)
add_subdirectory(server/)
add_subdirectory(license/)
add_subdirectory(MusicBot/)
add_subdirectory(file/)