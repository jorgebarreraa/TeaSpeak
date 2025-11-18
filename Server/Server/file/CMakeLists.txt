cmake_minimum_required(VERSION 3.6)
project(TeaSpeak-Files)

#set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(TeaSpeak-FileServer STATIC
        local_server/LocalFileProvider.cpp
        local_server/LocalFileSystem.cpp
        local_server/LocalFileTransfer.cpp
        local_server/LocalFileTransferClientWorker.cpp
        local_server/LocalFileTransferDisk.cpp
        local_server/LocalFileTransferNetwork.cpp
        local_server/clnpath.cpp
        local_server/NetTools.cpp
        local_server/Config.cpp
        local_server/HTTPUtils.cpp
)

add_library(TeaSpeak-FileRust STATIC
        local_rust/provider.cpp
        local_rust/lib.cpp
)
target_include_directories(TeaSpeak-FileRust PUBLIC include/)
target_link_libraries(TeaSpeak-FileRust PUBLIC TeaSpeak ${CMAKE_SOURCE_DIR}/file-rust/target/debug/libteaspeak_file_capi.so)
# target_link_directories(TeaSpeak-FileRust PUBLIC ${CMAKE_SOURCE_DIR}/file-rust/target/debug/)

target_link_libraries(TeaSpeak-FileServer PUBLIC TeaSpeak ${StringVariable_LIBRARIES_STATIC} stdc++fs
        libevent::core libevent::pthreads
        # We're not linking this here, since we may later use DataPipes::shared linking
        # DataPipes::core::static
        openssl::ssl::shared
        openssl::crypto::shared
)

target_include_directories(TeaSpeak-FileServer PUBLIC include/)
target_compile_options(TeaSpeak-FileServer PUBLIC "-Wswitch-enum")
target_compile_features(TeaSpeak-FileServer PUBLIC cxx_std_20)

add_executable(TeaSpeak-FileServerTest test/main.cpp)
target_link_libraries(TeaSpeak-FileServerTest PUBLIC TeaSpeak-FileServer
        TeaMusic            #Static (Must be in here, so we link against TeaMusic which uses C++11. That forbids GCC to use the newer glibc version)
        CXXTerminal::static
        DataPipes::core::static
        stdc++fs
)
target_compile_options(TeaSpeak-FileServerTest PUBLIC -static-libgcc -static-libstdc++)

add_executable(FileServer-CLNText local_server/clnpath.cpp)
target_compile_definitions(FileServer-CLNText PUBLIC -DCLN_EXEC)