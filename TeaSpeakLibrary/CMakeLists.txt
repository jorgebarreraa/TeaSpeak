cmake_minimum_required(VERSION 3.6)
project(TeaSpeak-Shared)
set(CMAKE_CXX_STANDARD 20)

if(NOT WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wno-reorder -Wno-sign-compare -fpermissive -ftemplate-depth=1000 ${MEMORY_DEBUG_FLAGS}")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
else()
    #For Windows
    add_definitions(-D_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING)

    add_compile_options(/wd4996) #'std::result_of_t': warning STL4014: std::result_of and std::result_of_t are deprecated in C++17.
endif()

if(CMAKE_PLATFORM_INCLUDE AND NOT CMAKE_PLATFORM_INCLUDE STREQUAL "")
    include(${CMAKE_PLATFORM_INCLUDE})
endif()

find_package(TomMath REQUIRED)
include_directories(${TomMath_INCLUDE_DIR})

find_package(TomCrypt REQUIRED)
include_directories(${TomCrypt_INCLUDE_DIR})

find_package(DataPipes REQUIRED)
include_directories(${DataPipes_INCLUDE_DIR})

# LibEvent fucks up the CMAKE_FIND_LIBRARY_SUFFIXES variable
if (NOT find_event)
    function(find_event static)
        if(static)
            set(LIBEVENT_STATIC_LINK TRUE)
        endif()
        find_package(Libevent REQUIRED)
        include_directories(${LIBEVENT_INCLUDE_DIRS})
    endfunction()
endif ()
find_event(ON)

find_package(StringVariable REQUIRED)
include_directories(${StringVariable_INCLUDE_DIR})

find_package(Ed25519 REQUIRED)
include_directories(${ed25519_INCLUDE_DIR})

find_package(ThreadPool REQUIRED)
include_directories(${ThreadPool_INCLUDE_DIR})
if(WIN32)
    add_definitions(-DWINDOWS) #Required for ThreadPool
endif()
find_package(spdlog REQUIRED)
link_libraries(spdlog::spdlog_header_only) #Its a header only lib so we should be fine :)

if(NOT TEASPEAK_SERVER)
    add_definitions(-DNO_OPEN_SSL)
    add_definitions(-D_HAS_STD_BYTE)
    #FML
else()
    find_package(CXXTerminal REQUIRED)

    add_definitions(-DHAVE_CXX_TERMINAL)
    add_definitions(-DHAVE_JSON)
    set(HAVE_SQLITE3 ON)
    set(HAVE_OPEN_SSL ON)
    message("HAVE JSON!")
endif()

if (MSVC)
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
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()
	add_compile_options("/EHsc") #We require exception handling
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3") #-DNDEBUG We want assert!
endif()

# TODO: Reenable for the TeaClient!
#add_definitions(-DUSE_BORINGSSL)
#include_directories(${LIBRARY_PATH}/boringssl/include/)

set(SOURCE_FILES
        src/misc/rnd.cpp
        src/misc/time.cpp
        src/misc/memtracker.cpp
        src/misc/digest.cpp
        src/misc/base64.cpp
        src/misc/net.cpp
        src/misc/task_executor.cpp
        src/misc/threads.cpp

        src/lock/rw_mutex.cpp

        #Logger
        src/log/LogUtils.cpp
        src/log/LogSinks.cpp


        src/qlz/QuickLZ.cpp
        src/qlz/QuickLZ_L3.cpp
        src/qlz/QuickLZ_L1.cpp

        src/converters/converter.cpp

        src/query/command3.cpp
        src/query/command2.cpp
        src/query/Command.cpp
        src/query/escape.cpp

        src/protocol/generation.cpp
        src/protocol/Packet.cpp
        src/protocol/buffers.cpp
        src/protocol/buffers_allocator_c.cpp
        src/PermissionManager.cpp
        src/Properties.cpp
        src/BasicChannel.cpp
        src/Error.cpp
        src/protocol/CryptHandler.cpp
        src/protocol/CompressionHandler.cpp
        src/Variable.cpp
        src/linked_helper.cpp
        src/EventLoop.cpp

        src/License.cpp
        src/PropertyDefinitions.cpp

        src/bbcode/bbcodes.cpp

        src/channel/TreeView.cpp
        src/protocol/ringbuffer.cpp
        src/protocol/AcknowledgeManager.cpp
        src/protocol/PacketLossCalculator.cpp
        src/protocol/PingHandler.cpp
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
        src/misc/memtracker.h
        src/misc/strobf.h
        src/misc/task_executor.h
        src/misc/threads.h

        src/log/translation.h
        src/log/LogUtils.h

        src/PermissionManager.h
        src/protocol/buffers.h
        src/protocol/Packet.h
        src/Properties.h
        src/BasicChannel.h
        src/Definitions.h
        src/Error.h
        src/protocol/CryptHandler.h
        src/Variable.h
        src/misc/queue.h

        src/misc/digest.h

        src/bbcode/bbcodes.h

        src/channel/TreeView.h
)

if(HAVE_SQLITE3)
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
    set(SOURCE_FILES ${SOURCE_FILES}
        src/ssl/SSLManager.cpp
    )
    set(HEADER_FILES ${HEADER_FILES}
        src/ssl/SSLManager.h
    )
    set(OPENSSL_LIBRARIES
            openssl::ssl::static
            openssl::crypto::static)
endif()

add_library(TeaSpeak STATIC ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(TeaSpeak PUBLIC
        threadpool::static jsoncpp_lib
        ${OPENSSL_LIBRARIES}
        tomcrypt::static
        tommath::static
        dl
)

find_package(mysql REQUIRED)
set(mysql_FOUND ON)
message("${mysql_FOUND}")
if(mysql_FOUND)
    message("Found MySQL")
    target_link_libraries(TeaSpeak PUBLIC
            mysql::client::static
    )
    target_compile_options(TeaSpeak PRIVATE "-Wall" "-DHAVE_MYSQL_H")
else()
    message("Building without MySQL Support")
endif()

if (TEASPEAK_SERVER)
    target_link_libraries(TeaSpeak PUBLIC CXXTerminal::static)
endif ()
target_include_directories(TeaSpeak PUBLIC src/)
install(TARGETS TeaSpeak
        ARCHIVE DESTINATION lib
)
INSTALL (
        DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h*"
)

set(TEST_LIBRARIES
        threadpool::static    #Static
        TeaSpeak            #Static
        TeaLicenseHelper    #Static
        TeaMusic            #Static
        CXXTerminal::static  #Static
        ${StringVariable_LIBRARIES_STATIC}
        ${YAML_CPP_LIBRARIES}
        pthread
        stdc++fs
        libevent::core libevent::pthreads
        opus::static
        yaml-cpp
        ${LIBRARY_PATH_PROTOBUF}

        #We're forsed to use boringssl caused by the fact that boringssl is already within webrtc!

        #Require a so
        sqlite3

        breakpad::static
        protobuf::libprotobuf
        jemalloc::shared

        tomcrypt::static
        tommath::static

        mysqlclient.a
        jsoncpp_lib
        ${ed25519_LIBRARIES_STATIC}
        ${DataPipes_LIBRARIES_SHARED} # Also includes glib2.0

        openssl::ssl::shared
        openssl::crypto::shared
        dl
        z
)

include_directories(src/)
option(BUILD_TESTS "Enable/disable test building" ON)
if(BUILD_TESTS)
    add_executable(RingTest test/RingTest.cpp ${SOURCE_FILES})
    target_link_libraries(RingTest ${TEST_LIBRARIES})

    if(NOT WIN32)
        add_executable(CommandTest test/CommandTest.cpp src/query/command3.cpp src/query/Command.cpp src/query/escape.cpp src/converters/converter.cpp src/Variable.cpp)
        target_link_libraries(CommandTest DataPipes::core::shared jsoncpp_lib ${glib20_DIR}/lib/x86_64-linux-gnu/libffi.so.7 ${nice_DIR}/lib/libnice.so.10)

        add_executable(WebsocketTest ${SOURCE_FILES} ${HEADER_FILES} test/WSSTest.cpp src/log/LogSinks.cpp src/log/LogSinks.h)
        target_link_libraries(WebsocketTest ${TEST_LIBRARIES})

        #add_executable(SQLTest ${SOURCE_FILES} ${HEADER_FILES} test/SQLTest.cpp src/log/LogSinks.cpp src/log/LogSinks.h)
        #target_link_libraries(SQLTest ${TEST_LIBRARIES})
        add_executable(SQL2Test test/SQL2Test.cpp src/Variable.cpp src/misc/net.cpp)
        target_link_libraries(SQL2Test sqlite3)

        add_executable(ChannelTest ${SOURCE_FILES} ${HEADER_FILES} test/ChannelTest.cpp src/log/LogSinks.cpp src/log/LogSinks.h)
        target_link_libraries(ChannelTest ${TEST_LIBRARIES})

        add_executable(EndianessTest ${SOURCE_FILES} ${HEADER_FILES} test/EndianessTest.cpp src/log/LogSinks.cpp src/log/LogSinks.h)
        target_link_libraries(EndianessTest ${TEST_LIBRARIES})

        include_directories(/usr/local/include/breakpad)
        add_executable(CrashTest test/CrashTest.cpp ${SOURCE_FILES})
        target_link_libraries(CrashTest ${TEST_LIBRARIES})

        add_executable(PorpertyTest test/PropertyTest.cpp src/Properties.cpp src/PropertyDefinitions.cpp)
        target_link_libraries(PorpertyTest ${TEST_LIBRARIES})

        add_executable(BBTest test/BBTest.cpp ${SOURCE_FILES} src/query/command_unused.h src/misc/ip_router.cpp src/misc/ip_router.h)
        target_link_libraries(BBTest ${TEST_LIBRARIES})

        add_executable(LinkedTest test/LinkedTest.cpp ${SOURCE_FILES})
        target_link_libraries(LinkedTest ${TEST_LIBRARIES})

        add_executable(PermissionTest test/PermissionTest.cpp ${SOURCE_FILES})
        target_link_libraries(PermissionTest ${TEST_LIBRARIES})

        add_executable(GenerationTest test/generationTest.cpp src/protocol/generation.cpp)
        target_link_libraries(GenerationTest ${TEST_LIBRARIES})

        add_executable(RW-Lock-Test test/rw_lock.cpp src/lock/rw_mutex.cpp)
        target_link_libraries(GenerationTest ${TEST_LIBRARIES})

        add_executable(PacketLossTest src/protocol/PacketLossCalculator.cpp test/PacketLossCalculateTest.cpp)
        target_link_libraries(PacketLossTest)

        add_executable(IP-Router-Test test/ip_router.cpp src/misc/ip_router.cpp)
    endif()
endif()
