cmake_minimum_required(VERSION 3.6)
project(TeaMusic)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fpermissive -Wall -Wno-sign-compare -Wno-reorder -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "${CMAKE_CXX_FLAGS_RELEASE} -O3")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/enviroment/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/libs/)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(../music/include/)
add_definitions(-DLTM_DESC)

#The basic library
add_library(TeaMusic SHARED src/MusicPlayer.cpp)
target_link_libraries(TeaMusic PUBLIC TeaSpeak libevent::core libevent::pthreads dl)

#The test file
add_executable(TeaMusicTest ${MUSIC_SOURCE_FILES} main.cpp)
target_link_libraries(TeaMusicTest ProviderFFMpeg ProviderYT TeaMusic pthread asound dl stdc++fs)
#ThreadPool opus asound opusfile stdc++fs dl TeaSpeak CXXTerminal event jsoncpp