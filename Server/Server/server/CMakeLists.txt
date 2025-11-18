cmake_minimum_required(VERSION 3.6)
project(TeaSpeak-Server)

set(CMAKE_VERBOSE_MAKEFILE ON)
#--allow-multiple-definition
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -Wall -Wno-reorder -Wno-sign-compare -static-libgcc -static-libstdc++ -g -Wl,--no-whole-archive -pthread ${MEMORY_DEBUG_FLAGS} -Werror=return-type -Werror=delete-non-virtual-dtor")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")

#NDEBUG
#set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/environment/)

#disable for debug
#add_definitions(-DRELEASE_MODE)
find_event(ON)

include_directories(../music/include/)
include_directories(../shared/src)
include_directories(../license/src)
include_directories(../MusicBot/src)
# include_directories(/usr/local/include/breakpad)
# include_directories(${LIBRARY_PATH}/tomcrypt/src/headers)

add_definitions(-DLTM_DESC)
add_definitions(-DMUSIC_BOT)
add_definitions(-DUSE_BORINGSSL)

#0 = STABLE
#1 = BETA
#2 = ALPHA
#3 = PRIVATE
option(BUILD_TYPE "Sets the build type" OFF)
option(BUILD_TYPE_NAME "Sets the build type name" OFF)
option(COMPILE_WEB_CLIENT "Enable/Disable the web cleint future" OFF)
#set(COMPILE_WEB_CLIENT "ON")

set(CMAKE_VERBOSE_MAKEFILE ON)
set(SERVER_SOURCE_FILES
        main.cpp
        tomcryptTest.cpp
        # Only needed for boringssl
        # MySQLLibSSLFix.c

        src/client/ConnectedClient.cpp
        src/server/PrecomputedPuzzles.cpp
        src/client/voice/VoiceClient.cpp
        src/client/voice/VoiceClientCommandHandler.cpp
        src/client/voice/VoiceClientConnectionPacketHandler.cpp
        src/TS3ServerClientManager.cpp
        src/VirtualServer.cpp
        src/FileServerHandler.cpp
        src/TS3ServerHeartbeat.cpp
        src/SignalHandler.cpp
        src/server/VoiceServer.cpp
        src/server/VoiceServerSocket.cpp
        src/server/POWHandler.cpp
        src/client/voice/VoiceClientConnection.cpp
        src/client/command_handler/groups.cpp
        src/client/command_handler/channel.cpp
        src/client/command_handler/client.cpp
        src/client/command_handler/server.cpp
        src/client/command_handler/misc.cpp
        src/client/command_handler/bulk_parsers.cpp

        src/client/ConnectedClientNotifyHandler.cpp
        src/VirtualServerManager.cpp
        src/channel/ServerChannel.cpp
        src/channel/ClientChannelView.cpp
        src/manager/BanManager.cpp
        src/client/InternalClient.cpp

        src/client/DataClient.cpp
        src/server/QueryServer.cpp
        src/client/query/QueryClient.cpp
        src/client/query/QueryClientCommands.cpp
        src/client/query/QueryClientNotify.cpp

        src/manager/IpListManager.cpp
        src/server/GlobalNetworkEvents.cpp

        src/ConnectionStatistics.cpp

        src/manager/TokenManager.cpp

        src/terminal/CommandHandler.cpp

        src/manager/ComplainManager.cpp
        src/DatabaseHelper.cpp

        src/manager/LetterManager.cpp
        src/manager/PermissionNameManager.cpp

        src/ServerManagerSnapshot.cpp
        src/ServerManagerSnapshotDeploy.cpp
        src/client/music/Song.cpp
        src/music/PlayablePlaylist.cpp
        src/InstanceHandler.cpp
        src/InstanceHandlerSetup.cpp
        src/InstanceHandlerPermissions.cpp
        src/PermissionCalculator.cpp

        src/Configuration.cpp

        src/build.cpp

        src/music/MusicPlaylist.cpp
        src/client/music/MusicClient.cpp
        src/client/music/MusicClientPlayer.cpp
        src/client/ConnectedClientTextCommandHandler.cpp
        src/music/MusicBotManager.cpp
        src/client/music/internal_provider/channel_replay/ChannelProvider.cpp

        src/geo/GeoLocation.cpp
        src/geo/IP2Location.cpp
        src/geo/VPNBlocker.cpp

        src/client/query/XMacroEventTypes.h

        src/client/SpeakingClient.cpp

        ../shared/src/ssl/SSLManager.cpp

        src/manager/SqlDataManager.cpp

        src/ShutdownHelper.cpp
        src/lincense/TeamSpeakLicense.cpp

        src/snapshots/permission.cpp
        src/snapshots/client.cpp
        src/snapshots/channel.cpp
        src/snapshots/server.cpp
        src/snapshots/groups.cpp
        src/snapshots/deploy.cpp
        src/snapshots/music.cpp
        src/snapshots/parser.cpp

        src/groups/Group.cpp
        src/groups/GroupManager.cpp
        src/groups/GroupAssignmentManager.cpp

        src/manager/ActionLogger.cpp
        src/manager/ActionLoggerImpl.cpp

        src/manager/ConversationManager.cpp
        src/client/SpeakingClientHandshake.cpp
        src/client/command_handler/music.cpp src/client/command_handler/file.cpp

        src/client/voice/PacketEncoder.cpp
        src/client/shared/ServerCommandExecutor.cpp
        src/client/voice/CryptSetupHandler.cpp
        src/client/shared/WhisperHandler.cpp

        src/terminal/PipedTerminal.cpp

        src/server/voice/UDPVoiceServer.cpp
        src/server/voice/DatagramPacket.cpp

        src/rtc/lib.cpp
)

if (COMPILE_WEB_CLIENT)
    add_definitions(-DCOMPILE_WEB_CLIENT)

    set(SERVER_SOURCE_FILES
            ${SERVER_SOURCE_FILES}

            src/server/WebServer.cpp
            src/client/web/WebClient.cpp
            src/client/web/WSWebClient.cpp
            src/client/command_handler/helpers.h src/music/PlaylistPermissions.cpp src/music/PlaylistPermissions.h src/lincense/LicenseService.cpp src/lincense/LicenseService.h)
endif ()

add_executable(PermHelper helpers/permgen.cpp)
target_link_libraries(PermHelper
        TeaSpeak
        stdc++fs
        CXXTerminal::static
        libevent::core libevent::pthreads
        ${StringVariable_LIBRARIES_STATIC}
)

add_executable(PermMapHelper helpers/PermMapGen.cpp)
target_link_libraries(PermMapHelper
        ${LIBRARY_PATH_ED255}

        TeaSpeak            #Static
        TeaLicenseHelper    #Static
        TeaMusic            #Static
        ${LIBRARY_PATH_THREAD_POOL}    #Static
        ${LIBRARY_PATH_TERMINAL}   #Static
        ${LIBRARY_PATH_VARIBALES}
        ${LIBRARY_PATH_YAML}
        pthread
        stdc++fs
        ${LIBEVENT_PATH}/libevent.a
        ${LIBEVENT_PATH}/libevent_pthreads.a
        ${LIBRARY_PATH_OPUS}
        ${LIBRARY_PATH_JSON}
        ${LIBRARY_PATH_PROTOBUF}

        #${LIBWEBRTC_LIBRARIES} #ATTENTIAN! WebRTC does not work with crypto! (Already contains a crypto version)
        ${LIBRARY_TOM_CRYPT}
        ${LIBRARY_TOM_MATH}

        #We're forsed to use boringssl caused by the fact that boringssl is already within webrtc!

        #Require a so
        sqlite3

        ${LIBRARY_PATH_BREAKPAD}
        ${LIBRARY_PATH_JDBC}
        ${LIBRARY_PATH_PROTOBUF}

        ${LIBRARY_PATH_DATA_PIPES}
        ${LIBRARY_PATH_BORINGSSL_SSL}
        ${LIBRARY_PATH_BORINGSSL_CRYPTO}
        dl
        jemalloc
        )


SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "5")
SET(CPACK_PACKAGE_VERSION_PATCH "6")
if (BUILD_TYPE_NAME EQUAL OFF)
    SET(CPACK_PACKAGE_VERSION_DATA "beta")
elseif (BUILD_TYPE_NAME STREQUAL "")
    SET(CPACK_PACKAGE_VERSION_DATA "")
else ()
    SET(CPACK_PACKAGE_VERSION_DATA "-${BUILD_TYPE_NAME}")
endif ()
if (NOT BUILD_TYPE AND NOT BUILD_TYPE STREQUAL "0")
    SET(BUILD_TYPE "3")
endif ()
set_source_files_properties(src/build.cpp PROPERTIES
        COMPILE_FLAGS "-DBUILD_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DBUILD_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBUILD_PATCH=${CPACK_PACKAGE_VERSION_PATCH} -DBUILD_DATA=\"${CPACK_PACKAGE_VERSION_DATA}\" -DBUILD_TYPE=${BUILD_TYPE} -DBUILD_COUNT=0")
file(WRITE repro/env/buildVersion.txt "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}${CPACK_PACKAGE_VERSION_DATA}")

add_executable(TeaSpeakServer ${SERVER_SOURCE_FILES})
target_link_libraries(TeaSpeakServer
        threadpool::static    #Static
        TeaSpeak            #Static
        TeaLicenseHelper    #Static
        TeaMusic            #Static
        CXXTerminal::static  #Static
        TeaSpeak-FileServer
        # TeaSpeak-FileRust
        ${StringVariable_LIBRARIES_STATIC}
        ${YAML_CPP_LIBRARIES}
        pthread
        stdc++fs
        libevent::core libevent::pthreads
        opus::static
        yaml-cpp

        #Require a so
        sqlite3
        DataPipes::core::static

        breakpad::static
        protobuf::libprotobuf
        #jemalloc::shared

        tomcrypt::static
        tommath::static

        jsoncpp_lib
        zstd::libzstd_static
)

if(EXISTS "${CMAKE_SOURCE_DIR}/rtclib/libteaspeak_rtc.so" AND NOT NO_RELEASE_RTC)
   message("Linking to release librtc file")
   target_link_libraries(TeaSpeakServer
           ${CMAKE_SOURCE_DIR}/rtclib/libteaspeak_rtc.so
   )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/rtclib/libteaspeak_rtc_development.so")
    message("Linkding against debug libteaspeak_rtc_development.so")
    target_link_libraries(TeaSpeakServer
        ${CMAKE_SOURCE_DIR}/rtclib/libteaspeak_rtc_development.so
    )
else()
    message(FATAL_ERROR "Missing librtc library file")
endif()

# include_directories(${LIBRARY_PATH}/boringssl/include/)
target_link_libraries(TeaSpeakServer
        openssl::ssl::shared
        openssl::crypto::shared
        dl
        z
        rt # For clock_gettime
)

#check_include_file(mysql.h HAVE_MYSQL_MYSQL_H)
#if (NOT HAVE_MYSQL_MYSQL_H)
#    check_include_file(mysql.h HAVE_MYSQL_H)
#    if (NOT HAVE_MYSQL_H)
#        message(FATAL_ERROR "Missing MySQL header")
#    endif ()
#endif ()

set(DISABLE_JEMALLOC ON)
if (NOT DISABLE_JEMALLOC)
    target_link_libraries(TeaSpeakServer
            jemalloc
    )
    add_definitions(-DHAVE_JEMALLOC)
endif ()


add_executable(Snapshots-Permissions-Test src/snapshots/permission.cpp tests/snapshots/permission.cpp)
target_link_libraries(Snapshots-Permissions-Test PUBLIC
    TeaSpeak
    CXXTerminal::static  #Static
    ${StringVariable_LIBRARIES_STATIC}
    ${YAML_CPP_LIBRARIES}
    pthread
    stdc++fs
    libevent::core libevent::pthreads

    #Require a so
    sqlite3
    DataPipes::core::static

    tomcrypt::static
    tommath::static
)
target_include_directories(Snapshots-Permissions-Test PUBLIC ${CMAKE_SOURCE_DIR}/server/src/)