cmake_minimum_required(VERSION 3.6)
project(TeaSpeakLicence)

# Force C++11 for this module (override parent CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fpermissive -Wall -Wno-sign-compare -Wno-reorder -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/environment/)

#disable for debug
#add_definitions(-DRELEASE_MODE)

include_directories(../shared/src)
add_definitions(-DLTM_DESC)

set(LICENCE_SOURCE_FILES
		shared/src/license.cpp
		shared/src/client.cpp
)

#Protobuf
find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
protobuf_generate_cpp(PROTO_SRCS TeaLicenseHelper_PROTOCOL_HEADERS shared/packets/LicenseRequest.proto shared/packets/LicenseManager.proto)

#Ed25519
find_package(Ed25519 REQUIRED)
include_directories(${ed25519_INCLUDE_DIR})

#The actual shared library
add_library(TeaLicenseHelper STATIC ${LICENCE_SOURCE_FILES} ${PROTO_SRCS})
target_link_libraries(TeaLicenseHelper PUBLIC
		TeaSpeak
		protobuf::libprotobuf
		libevent::core libevent::pthreads
		# openssl::ssl::shared
		# openssl::crypto::shared

		${StringVariable_LIBRARIES_STATIC}
		${ed25519_LIBRARIES_STATIC}
		stdc++fs
		rt
)
target_include_directories(TeaLicenseHelper PUBLIC shared/include/)
target_include_directories(TeaLicenseHelper PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

#The license server
add_executable(TeaLicenseServer ${LICENCE_SOURCE_FILES}
		server/KeyIdCache.cpp
		server/LicenseServer.cpp
		server/LicenseServerHandler.cpp
        server/DatabaseHandler.cpp
		server/WebAPI.cpp
		server/StatisticManager.cpp
		server/UserManager.cpp
		LicenseServerMain.cpp
		# Only for boringssl: MySQLLibSSLFix.c
)

target_link_libraries(TeaLicenseServer
		TeaSpeak
		TeaLicenseHelper    #Static
        threadpool::static    #Static
        CXXTerminal::static  #Static
        libevent::core libevent::pthreads

        mysqlclient
        jsoncpp_lib
		DataPipes::core::static
        openssl::ssl::static
        openssl::crypto::static

        pthread
        dl
		z
		rt
)

#The test license client
add_executable(TeaLicenseClient LicenseClientMain.cpp)
target_link_libraries(TeaLicenseClient TeaLicenseHelper)

#The license manager
if(NOT DISABLE_QT)
	find_package(Qt5Widgets)
	include_directories(${Qt5Widgets_INCLUDE_DIRS})
	add_definitions(${Qt5Widgets_DEFINITIONS})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")

	set(UI_FILES manager/ui/licensegenerator.ui manager/ui/loginwindow.ui manager/ui/owerview.ui manager/ui/licenseinfo.ui)
	set(DESIGN_HEADER_FILES manager/ui/LicenseGenerator.h manager/ui/LoginWindow.h manager/ui/Overview.h manager/ui/UiLicenseInfo.h)

	qt5_wrap_ui(UI_WARPED_FILES ${UI_FILES})
	qt5_wrap_cpp(UI_HEADER_FILES ${DESIGN_HEADER_FILES})

	add_executable(LicenseManager
		${LICENCE_SOURCE_FILES}
		LicenseManager.cpp
		manager/ui/LicenseGenerator.cpp
		manager/ui/LoginWindow.cpp
		manager/ui/Overview.cpp
        manager/ui/UiLicenseInfo.cpp
		manager/ServerConnection.cpp
		manager/ServerConnectionHandler.cpp
		manager/ServerConnectionExecutor.cpp
		${UI_WARPED_FILES} ${UI_HEADER_FILES} ${PROTO_SRCS} ${PROTO_HDRS})
	target_link_libraries(LicenseManager Qt5::Widgets Qt5::Core Qt5::Gui)
	target_link_libraries(LicenseManager
		${LIBRARY_PATH_THREAD_POOL}    #Static
		TeaSpeak            #Static
		${LIBRARY_PATH_TERMINAL}   #Static
		${LIBRARY_PATH_BORINGSSL_CRYPTO}
		${PROJECT_SOURCE_DIR}/../../libraries/event/build/lib/libevent.a
		${PROJECT_SOURCE_DIR}/../../libraries/event/build/lib/libevent_pthreads.a
		pthread
		${LIBRARY_PATH_VARIBALES}
		${LIBRARY_PATH_BREAKPAD}
		${LIBRARY_PATH_PROTOBUF}

		${LIBRARY_TOM_MATH}
		${LIBRARY_TOM_CRYPT}
		${LIBRARY_PATH_ED255}
		stdc++fs
		jsoncpp.a
		${LIBRARY_PATH_DATA_PIPES}
		${LIBRARY_PATH_BORINGSSL_SSL}
		${LIBRARY_PATH_BORINGSSL_CRYPTO}
	)
endif()

add_executable(LicenseCLI
		LicenseCreatorCLI.cpp
		manager/ServerConnection.cpp
		manager/ServerConnectionExecutor.cpp
		manager/ServerConnectionHandler.cpp
)

target_link_libraries(LicenseCLI TeaLicenseHelper)