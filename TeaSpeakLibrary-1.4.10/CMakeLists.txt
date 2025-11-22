cmake_minimum_required(VERSION 3.6)
project(TeaSpeak-Shared)
set(CMAKE_CXX_STANDARD 20)

if(CMAKE_PLATFORM_INCLUDE AND NOT CMAKE_PLATFORM_INCLUDE STREQUAL "")
    include(${CMAKE_PLATFORM_INCLUDE})
endif()

find_package(TomMath REQUIRED)
include_directories(${TomMath_INCLUDE_DIR})

find_package(TomCrypt REQUIRED)
include_directories(${TomCrypt_INCLUDE_DIR})

find_package(DataPipes REQUIRED)
include_directories(${DataPipes_INCLUDE_DIR})

find_package(Ed25519 REQUIRED)

set(TARGET_LIBRARIES)
set(FEATURE_LOGGING ON)
set(FEATURE_DATABASE ON)

find_package(ThreadPool)
if(ThreadPool_FOUND)
    include_directories(${ThreadPool_INCLUDE_DIR})
    if(WIN32)
        add_definitions(-DWINDOWS) #Required for ThreadPool
    endif()
else()
    message("Missing ThreadPool. Skipping database support.")
    set(FEATURE_DATABASE OFF)
endif()

find_package(StringVariable)
if(StringVariable_FOUND)
    include_directories(${StringVariable_INCLUDE_DIR})
else()
    message("Missing StringVariable. Disabling logging support")
    set(FEATURE_LOGGING OFF)
endif()

find_package(spdlog)
if(spdlog_FOUND)
    #Its a header only lib so we should be fine :)
    list(APPEND TARGET_LIBRARIES spdlog::spdlog_header_only)
else()
    message("Missing spdlog. Disabling logging support")
    set(FEATURE_LOGGING OFF)
endif()

find_package(CXXTerminal)
if(CXXTerminal_FOUND)
    add_definitions(-DHAVE_CXX_TERMINAL)
else()
    message("Missing CXXTerminal. Disabling logging support")
    set(FEATURE_LOGGING OFF)
endif()

if(NOT TEASPEAK_SERVER)
    add_definitions(-DNO_OPEN_SSL)
    add_definitions(-D_HAS_STD_BYTE)
    #FML
else()
    set(HAVE_SQLITE3 ON)
    set(HAVE_OPEN_SSL ON)
endif()

if(FEATURE_DATABASE)
    if(NOT FEATURE_LOGGING)
        message("Disabling database support because logging support is omitted")
        set(FEATURE_DATABASE OFF)
    endif()
endif()

if (MSVC)
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)
    add_compile_options(/wd4996) #'std::result_of_t': warning STL4014: std::result_of and std::result_of_t are deprecated in C++17.

	set(CompilerFlags
			CMAKE_CXX_FLAGS
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
			CMAKE_C_FLAGS_RELWITHDEBINFO
			CMAKE_C_FLAGS
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
	)
	foreach(CompilerFlag ${CompilerFlags})
        string(REGEX REPLACE "/M[DT]d?" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
	add_compile_options("/EHsc") #We require exception handling
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wno-reorder -Wno-sign-compare -fpermissive -ftemplate-depth=1000 ${MEMORY_DEBUG_FLAGS}")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3") #-DNDEBUG We want assert!
endif()

set(SOURCE_FILES
        src/misc/rnd.cpp
        src/misc/duration_utils.cpp
        src/misc/digest.cpp
        src/misc/base64.cpp
        src/misc/net.cpp
        src/misc/task_executor.cpp

        src/lock/rw_mutex.cpp

        src/qlz/QuickLZ.cpp
        src/converters/converter.cpp

        src/query/command3.cpp
        src/query/command2.cpp
        src/query/Command.cpp
        src/query/escape.cpp

        src/protocol/generation.cpp
        src/protocol/Packet.cpp
        src/protocol/buffers.cpp
        src/protocol/buffers_allocator_c.cpp
        src/protocol/CryptHandler.cpp
        src/protocol/CompressionHandler.cpp
        src/protocol/ringbuffer.cpp
        src/protocol/AcknowledgeManager.cpp
        src/protocol/PacketLossCalculator.cpp
        src/protocol/RawCommand.cpp
        src/protocol/PacketDecoder.cpp
        src/protocol/PingHandler.cpp
        src/protocol/PacketStatistics.cpp
        src/protocol/RtoCalculator.cpp

        src/Properties.cpp
        src/Error.cpp
        src/Variable.cpp
        src/linked_helper.cpp
        src/EventLoop.cpp

        src/License.cpp
        src/PropertyDefinitions.cpp

        src/bbcode/bbcodes.cpp

        src/channel/TreeView.cpp
)

set(HEADER_FILES
        src/misc/base64.h
        src/misc/endianness.h
        src/misc/cast.h
        src/misc/rnd.h
        src/misc/time.h
        src/misc/std_unique_ptr.h
        src/misc/net.h
        src/misc/lambda.h
        src/misc/hex.h
        src/misc/advanced_mutex.h
        src/misc/strobf.h
        src/misc/task_executor.h

        src/protocol/buffers.h
        src/protocol/Packet.h
        src/Properties.h
        src/Definitions.h
        src/Error.h
        src/protocol/CryptHandler.h
        src/Variable.h
        src/misc/queue.h

        src/misc/digest.h

        src/bbcode/bbcodes.h

        src/channel/TreeView.h
)

if(NOT ThreadPool_FOUND)
    set(SOURCE_FILES ${SOURCE_FILES}
        src/misc/threads.cpp
    )
endif()

if(FEATURE_LOGGING)
    set(SOURCE_FILES ${SOURCE_FILES}
            src/log/LogUtils.cpp
            src/log/LogSinks.cpp
    )

    set(HEADER_FILES ${HEADER_FILES}
            src/log/LogUtils.h
            src/log/LogSinks.h
    )

    add_definitions(-DFEATURE_LOGGING)
endif()

if(FEATURE_LOGGING)
    set(SOURCE_FILES ${SOURCE_FILES} src/misc/memtracker.cpp)
    set(HEADER_FILES ${HEADER_FILES} src/misc/memtracker.h)
    add_definitions(-DFEATURE_MEMTRACK)
else()
    message("Missing logging support. Don't build mem tracker")
endif()

if(FEATURE_DATABASE)
    set(SOURCE_FILES ${SOURCE_FILES}
        src/sql/SqlQuery.cpp
        src/sql/sqlite/SqliteSQL.cpp
        src/sql/mysql/MySQL.cpp
    )
    set(HEADER_FILES ${HEADER_FILES}
        src/sql/SqlQuery.h
        src/sql/sqlite/SqliteSQL.h
        src/sql/mysql/MySQL.h
    )
endif()

if(HAVE_OPEN_SSL)
    set(SOURCE_FILES ${SOURCE_FILES} src/ssl/SSLManager.cpp)
    set(HEADER_FILES ${HEADER_FILES} src/ssl/SSLManager.h)
    set(OPENSSL_LIBRARIES openssl::ssl::shared openssl::crypto::shared)
endif()

if (TEASPEAK_SERVER)
    # TODO: Remove such stuff and move it into the server!
    message("Adding TeaSpeak server only related files")


    set(SOURCE_FILES ${SOURCE_FILES}
            src/PermissionManager.cpp
            src/BasicChannel.cpp
            src/lookup/ip.cpp
    )

    set(HEADER_FILES ${HEADER_FILES}
            src/BasicChannel.h
            src/PermissionManager.h
    )
endif ()

add_library(TeaSpeak STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(TeaSpeak PUBLIC
        tomcrypt::static
        tommath::static
        ed25519::static
        ${OPENSSL_LIBRARIES}
        ${TARGET_LIBRARIES}
        dl
)

if (TEASPEAK_SERVER)
    find_package(mysql REQUIRED)
    message("Found MySQL")
    target_link_libraries(TeaSpeak PUBLIC
            mysql::client::static
    )
    target_compile_options(TeaSpeak PRIVATE "-Wall" "-DHAVE_MYSQL_H")

    target_link_libraries(TeaSpeak PUBLIC CXXTerminal::static)
endif ()

target_include_directories(TeaSpeak PUBLIC src/)

install(TARGETS TeaSpeak
        ARCHIVE DESTINATION lib
)

INSTALL(
        DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h*"
)
